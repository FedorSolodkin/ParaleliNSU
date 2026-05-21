#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Псевдоним для высокоточных часов
using WallClock = std::chrono::high_resolution_clock;

// Читаем MemAvailable из /proc/meminfo (Linux), возвращаем значение в ГБ.
// Нужно чтобы не пытаться выделить матрицу 12 ГБ на машине с 4 ГБ ОЗУ.
long long free_memory_gb() {
    std::ifstream memfile("/proc/meminfo");
    std::string field;
    long long kbytes;
    while (memfile >> field >> kbytes) {
        if (field == "MemAvailable:") return kbytes / (1024LL * 1024LL);
    }
    return 999; // не удалось прочитать — не блокируем выполнение
}

// Параллельная инициализация матрицы A (rows×cols) и вектора-результата out.
// Каждый поток инициализирует свой диапазон строк — это важно для NUMA:
// «first-touch» политика Linux размещает страницу памяти на том NUMA-узле,
// который первым к ней обратился. Инициализируя в параллельных потоках,
// мы распределяем данные по узлам так же, как будет работать умножение.
void parallel_init(std::vector<double>& A, std::vector<double>& vec,
                   std::vector<double>& out, int rows, int cols, int nthreads) {
    std::vector<std::thread> workers;
    workers.reserve(nthreads);

    for (int tid = 0; tid < nthreads; ++tid) {
        // Равномерно делим строки матрицы между потоками
        int row_beg = static_cast<long long>(tid)     * rows / nthreads;
        int row_end = static_cast<long long>(tid + 1) * rows / nthreads;

        workers.emplace_back([&A, &out, row_beg, row_end, cols, tid]() {
            // Каждый поток имеет свой генератор (seed = tid+1) — детерминировано
            std::mt19937_64 gen(static_cast<uint64_t>(tid) + 1);
            std::uniform_real_distribution<double> rnd(-1.0, 1.0);
            for (int i = row_beg; i < row_end; ++i) {
                double* row_ptr = A.data() + static_cast<long long>(i) * cols;
                for (int j = 0; j < cols; ++j) row_ptr[j] = rnd(gen);
                out[i] = 0.0; // обнуляем вектор результата
            }
        });
    }

    // Вектор x маленький (≤ 320 КБ при cols=40000) — инициализируем в главном потоке
    std::mt19937_64 gen_x(9999);
    std::uniform_real_distribution<double> rnd_x(-1.0, 1.0);
    for (int j = 0; j < cols; ++j) vec[j] = rnd_x(gen_x);

    for (auto& w : workers) w.join();
}

// Параллельное умножение матрицы A (rows×cols) на вектор vec, результат → out.
// Возвращает время выполнения в секундах.
// Каждый поток обрабатывает свой диапазон строк — нет общих данных для записи,
// поэтому синхронизация не нужна (race-free by construction).
double parallel_matvec(const std::vector<double>& A, const std::vector<double>& vec,
                       std::vector<double>& out, int rows, int cols, int nthreads) {
    std::vector<std::thread> workers;
    workers.reserve(nthreads);

    auto start_ts = WallClock::now();

    for (int tid = 0; tid < nthreads; ++tid) {
        int row_beg = static_cast<long long>(tid)     * rows / nthreads;
        int row_end = static_cast<long long>(tid + 1) * rows / nthreads;

        workers.emplace_back([&A, &vec, &out, row_beg, row_end, cols]() {
            for (int i = row_beg; i < row_end; ++i) {
                const double* row_ptr = A.data() + static_cast<long long>(i) * cols;
                double acc = 0.0; // накопитель скалярного произведения
                for (int j = 0; j < cols; ++j) acc += row_ptr[j] * vec[j];
                out[i] = acc;
            }
        });
    }
    for (auto& w : workers) w.join();

    return std::chrono::duration<double>(WallClock::now() - start_ts).count();
}

// Проверка корректности: однопоточный и 4-поточный результаты должны совпадать.
// Запускаем на маленькой матрице 512×512, чтобы не ждать долго.
bool check_correctness() {
    const int SZ = 512;
    std::vector<double> A(SZ * SZ), vec(SZ), ref(SZ), par(SZ);
    parallel_init(A, vec, ref, SZ, SZ, 1);
    par = ref; // одинаковые начальные условия
    parallel_matvec(A, vec, ref, SZ, SZ, 1);
    parallel_matvec(A, vec, par, SZ, SZ, 4);
    for (int i = 0; i < SZ; ++i)
        if (std::abs(ref[i] - par[i]) > 1e-9 * std::abs(ref[i]) + 1e-9)
            return false;
    return true;
}

// Основной бенчмарк для матрицы размером rows×cols.
// Для каждого числа потоков: 3 прогрева + 100 измерений → берём min и avg.
// Ускорение S_p = T(1) / T(p).
void run_benchmark(int rows, int cols, std::ofstream& csv) {
    const std::vector<int> thread_counts = {1, 2, 4, 7, 8, 16, 20, 40};
    constexpr int WARMUP_RUNS   = 3;
    constexpr int MEASURE_RUNS  = 100;

    // Оцениваем сколько памяти нужно (матрица A: rows*cols*8 байт)
    double size_gib = static_cast<double>(rows) * cols * sizeof(double) / (1LL << 30);
    long long needed_gb = static_cast<long long>(std::ceil(size_gib)) + 2;

    std::cout << "\n=== Матрица " << rows << "x" << cols
              << "  (A ≈ " << std::fixed << std::setprecision(1)
              << size_gib << " GiB) ===\n";

    long long avail_gb = free_memory_gb();
    if (avail_gb < needed_gb) {
        std::cout << "  Пропускаем: доступно " << avail_gb << " ГБ, нужно ~"
                  << needed_gb << " ГБ\n";
        return;
    }
    std::cout << "  Свободная память: " << avail_gb << " ГБ — достаточно\n\n";

    // Шапка таблицы
    std::cout << std::setw(8)  << "Потоки"
              << std::setw(12) << "T_min (с)"
              << std::setw(12) << "T_avg (с)"
              << std::setw(8)  << "S_p"
              << "\n" << std::string(40, '-') << "\n";

    // Выделяем память один раз, потом переиспользуем
    std::vector<double> A(static_cast<long long>(rows) * cols), vec(cols), out(rows);

    double base_min = 0.0, base_avg = 0.0; // время однопоточного выполнения

    for (int nthreads : thread_counts) {
        // Инициализируем с тем же числом потоков (NUMA first-touch)
        parallel_init(A, vec, out, rows, cols, nthreads);

        // Прогрев — загоняем данные в кэш, стабилизируем частоту CPU
        for (int r = 0; r < WARMUP_RUNS; ++r)
            parallel_matvec(A, vec, out, rows, cols, nthreads);

        // Замеры
        double elapsed_min = std::numeric_limits<double>::max();
        double elapsed_sum = 0.0;
        for (int r = 0; r < MEASURE_RUNS; ++r) {
            double t = parallel_matvec(A, vec, out, rows, cols, nthreads);
            elapsed_sum += t;
            if (t < elapsed_min) elapsed_min = t;
        }
        double elapsed_avg = elapsed_sum / MEASURE_RUNS;

        // Запоминаем базовое время (1 поток) для расчёта ускорения
        if (nthreads == 1) { base_min = elapsed_min; base_avg = elapsed_avg; }

        double speedup_min = base_min / elapsed_min;
        double speedup_avg = base_avg / elapsed_avg;

        std::cout << std::setw(8)  << nthreads
                  << std::setw(12) << std::fixed << std::setprecision(4) << elapsed_min
                  << std::setw(12) << std::fixed << std::setprecision(4) << elapsed_avg
                  << std::setw(8)  << std::fixed << std::setprecision(3) << speedup_min
                  << "\n" << std::flush;

        // Пишем в CSV сразу, чтобы не потерять данные при прерывании
        csv << rows << "," << nthreads << ","
            << std::fixed << std::setprecision(6) << elapsed_min << ","
            << std::fixed << std::setprecision(6) << elapsed_avg << ","
            << std::fixed << std::setprecision(4) << speedup_min << ","
            << std::fixed << std::setprecision(4) << speedup_avg << "\n";
        csv.flush();
    }
}

int main() {
    std::cout << "Бенчмарк: умножение матрицы на вектор (параллельная версия)\n";
    std::cout << "Аппаратных потоков: " << std::thread::hardware_concurrency() << "\n";

    std::cout << "\n[Проверка корректности 512x512] ";
    std::cout << (check_correctness() ? "PASSED" : "FAILED") << "\n";

    // CSV-файл: строка записывается сразу после каждого замера
    std::ofstream csv("results.csv");
    csv << "size,threads,t_min,t_avg,s_min,s_avg\n";

    run_benchmark(20000, 20000, csv);
    run_benchmark(40000, 40000, csv);

    std::cout << "\nРезультаты сохранены в results.csv\n";
    return 0;
}
