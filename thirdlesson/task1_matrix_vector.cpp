#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <mutex>

// Структура для хранения результатов теста
struct TestResult {
    int num_threads;
    double init_time;
    double compute_time;
    double total_time;
    double speedup;
    double efficiency;
};

// Параллельная инициализация матрицы и вектора
void parallel_init(std::vector<double>& matrix, std::vector<double>& vector, int size, int num_threads) {
    std::vector<std::thread> threads;
    unsigned long long elements_per_thread = (size * size + num_threads - 1) / num_threads;
    
    auto init_matrix = [&](int thread_id) {
        unsigned long long start = thread_id * elements_per_thread;
        unsigned long long end = std::min(start + elements_per_thread, (unsigned long long)size * size);
        
        for (unsigned long long i = start; i < end; ++i) {
            matrix[i] = static_cast<double>(i % 100) / 100.0;
        }
    };
    
    auto init_vector = [&](int thread_id) {
        unsigned long long start = thread_id * size / num_threads;
        unsigned long long end = (thread_id + 1) * size / num_threads;
        if (thread_id == num_threads - 1) end = size;
        
        for (unsigned long long i = start; i < end; ++i) {
            vector[i] = static_cast<double>(i % 50) / 50.0;
        }
    };
    
    // Запускаем потоки для инициализации матрицы
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(init_matrix, t);
    }
    for (auto& th : threads) th.join();
    threads.clear();
    
    // Запускаем потоки для инициализации вектора
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(init_vector, t);
    }
    for (auto& th : threads) th.join();
}

// Параллельное умножение матрицы на вектор
void parallel_mat_vec_mult(const std::vector<double>& matrix, const std::vector<double>& vector, 
                          std::vector<double>& result, int size, int num_threads) {
    std::vector<std::thread> threads;
    unsigned long long rows_per_thread = (size + num_threads - 1) / num_threads;
    
    auto compute_rows = [&](int thread_id) {
        unsigned long long start_row = thread_id * rows_per_thread;
        unsigned long long end_row = std::min(start_row + rows_per_thread, (unsigned long long)size);
        
        for (unsigned long long i = start_row; i < end_row; ++i) {
            double sum = 0.0;
            for (int j = 0; j < size; ++j) {
                sum += matrix[i * size + j] * vector[j];
            }
            result[i] = sum;
        }
    };
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(compute_rows, t);
    }
    for (auto& th : threads) th.join();
}

// Последовательная версия для сравнения
void sequential_mat_vec_mult(const std::vector<double>& matrix, const std::vector<double>& vector, 
                            std::vector<double>& result, int size) {
    for (int i = 0; i < size; ++i) {
        double sum = 0.0;
        for (int j = 0; j < size; ++j) {
            sum += matrix[i * size + j] * vector[j];
        }
        result[i] = sum;
    }
}

int main(int argc, char* argv[]) {
    int size = 20000; // Размер по умолчанию
    if (argc > 1) {
        size = std::atoi(argv[1]);
    }
    
    std::cout << "=== Анализ эффективности многопоточного умножения матрицы на вектор ===" << std::endl;
    std::cout << "Размер матрицы: " << size << "x" << size << std::endl;
    std::cout << std::endl;
    
    std::vector<int> thread_counts = {1, 2, 4, 7, 8, 16, 20, 40};
    std::vector<TestResult> results;
    
    // Базовый замер с 1 потоком
    std::vector<double> base_matrix(size * size);
    std::vector<double> base_vector(size);
    std::vector<double> base_result(size);
    
    auto start_total = std::chrono::high_resolution_clock::now();
    parallel_init(base_matrix, base_vector, size, 1);
    auto end_init = std::chrono::high_resolution_clock::now();
    parallel_mat_vec_mult(base_matrix, base_vector, base_result, size, 1);
    auto end_total = std::chrono::high_resolution_clock::now();
    
    double base_time = std::chrono::duration<double>(end_total - start_total).count();
    std::cout << "Базовое время (1 поток): " << std::fixed << std::setprecision(4) << base_time << " сек" << std::endl;
    std::cout << std::endl;
    
    // Тестирование для каждого количества потоков
    for (int num_threads : thread_counts) {
        std::cout << "--- Тестирование с " << num_threads << " потоками ---" << std::endl;
        
        std::vector<double> matrix(size * size);
        std::vector<double> vector(size);
        std::vector<double> result(size);
        
        // Замер времени инициализации
        auto start_init = std::chrono::high_resolution_clock::now();
        parallel_init(matrix, vector, size, num_threads);
        auto end_init = std::chrono::high_resolution_clock::now();
        double init_time = std::chrono::duration<double>(end_init - start_init).count();
        
        // Замер времени вычислений
        auto start_compute = std::chrono::high_resolution_clock::now();
        parallel_mat_vec_mult(matrix, vector, result, size, num_threads);
        auto end_compute = std::chrono::high_resolution_clock::now();
        double compute_time = std::chrono::duration<double>(end_compute - start_compute).count();
        
        double total_time = init_time + compute_time;
        double speedup = base_time / total_time;
        double efficiency = (speedup / num_threads) * 100.0;
        
        TestResult res{num_threads, init_time, compute_time, total_time, speedup, efficiency};
        results.push_back(res);
        
        std::cout << "  Инициализация: " << std::fixed << std::setprecision(4) << init_time << " сек" << std::endl;
        std::cout << "  Вычисления:    " << compute_time << " сек" << std::endl;
        std::cout << "  Общее время:   " << total_time << " сек" << std::endl;
        std::cout << "  Ускорение:     " << speedup << "x" << std::endl;
        std::cout << "  Эффективность: " << efficiency << "%" << std::endl;
        std::cout << std::endl;
    }
    
    // Генерация отчета
    std::cout << "=== Генерация отчета ===" << std::endl;
    
    // Создаем CSV файл с данными
    std::ofstream csv_file("report_data.csv");
    csv_file << "Threads,Init_Time,Compute_Time,Total_Time,Speedup,Efficiency\n";
    for (const auto& res : results) {
        csv_file << res.num_threads << "," 
                 << std::fixed << std::setprecision(6) << res.init_time << ","
                 << res.compute_time << ","
                 << res.total_time << ","
                 << res.speedup << ","
                 << res.efficiency << "\n";
    }
    csv_file.close();
    std::cout << "Создан файл report_data.csv с данными" << std::endl;
    
    // Создаем gnuplot скрипт
    std::ofstream gnuplot_file("plot_script.gnuplot");
    gnuplot_file << "set terminal pngcairo size 1200,800 enhanced font 'Arial,12'\n";
    gnuplot_file << "set output 'speedup_chart.png'\n";
    gnuplot_file << "set title 'Ускорение в зависимости от количества потоков' font 'Arial,16'\n";
    gnuplot_file << "set xlabel 'Количество потоков' font 'Arial,14'\n";
    gnuplot_file << "set ylabel 'Ускорение (x)' font 'Arial,14'\n";
    gnuplot_file << "set grid\n";
    gnuplot_file << "set key top left\n";
    gnuplot_file << "set xrange [0:45]\n";
    gnuplot_file << "set yrange [0:*]\n";
    gnuplot_file << "# Линия идеального ускорения\n";
    gnuplot_file << "f(x) = x\n";
    gnuplot_file << "plot f(x) title 'Идеальное ускорение' with lines linestyle 1 lc rgb 'gray' dt 2,\n";
    gnuplot_file << "     'report_data.csv' using 1:5 title 'Фактическое ускорение' with linespoints linewidth 3 pointtype 7 pointsize 1.5 lc rgb 'blue'\n";
    gnuplot_file.close();
    std::cout << "Создан скрипт plot_script.gnuplot" << std::endl;
    
    // Создаем текстовый отчет с подробным анализом
    std::ofstream report_file("detailed_report.txt");
    report_file << "================================================================================\n";
    report_file << "                    ОТЧЕТ ПО АНАЛИЗУ ЭФФЕКТИВНОСТИ\n";
    report_file << "              Многопоточное умножение матрицы на вектор\n";
    report_file << "================================================================================\n\n";
    
    report_file << "ПАРАМЕТРЫ ТЕСТА:\n";
    report_file << "  Размер матрицы: " << size << "x" << size << "\n";
    report_file << "  Тип данных: double\n";
    report_file << "  Количество потоков: 1, 2, 4, 7, 8, 16, 20, 40\n\n";
    
    report_file << "================================================================================\n";
    report_file << "                           ТАБЛИЦА РЕЗУЛЬТАТОВ\n";
    report_file << "================================================================================\n\n";
    
    report_file << std::setw(10) << "Потоки" 
                << std::setw(15) << "Иниц.(сек)"
                << std::setw(15) << "Вычисл.(сек)"
                << std::setw(15) << "Всего(сек)"
                << std::setw(12) << "Ускорение"
                << std::setw(15) << "Эффект.(%)" << "\n";
    report_file << std::string(82, '-') << "\n";
    
    for (const auto& res : results) {
        report_file << std::setw(10) << res.num_threads
                    << std::setw(15) << std::fixed << std::setprecision(4) << res.init_time
                    << std::setw(15) << res.compute_time
                    << std::setw(15) << res.total_time
                    << std::setw(12) << std::setprecision(2) << res.speedup
                    << std::setw(15) << std::setprecision(1) << res.efficiency << "\n";
    }
    
    report_file << "\n================================================================================\n";
    report_file << "                         ПОДРОБНЫЙ АНАЛИЗ ПО ГРУППАМ\n";
    report_file << "================================================================================\n\n";
    
    // Анализ для малых количеств потоков (1-4)
    report_file << "ГРУППА 1: Малое количество потоков (1-4)\n";
    report_file << "--------------------------------------------------------------------------------\n";
    for (const auto& res : results) {
        if (res.num_threads <= 4) {
            report_file << "  " << res.num_threads << " поток(а):\n";
            report_file << "    - Время выполнения: " << std::fixed << std::setprecision(4) << res.total_time << " сек\n";
            report_file << "    - Ускорение: " << std::setprecision(2) << res.speedup << "x\n";
            report_file << "    - Эффективность: " << std::setprecision(1) << res.efficiency << "%\n";
            if (res.efficiency > 90) {
                report_file << "    - Оценка: ОТЛИЧНО! Почти линейное ускорение.\n";
            } else if (res.efficiency > 70) {
                report_file << "    - Оценка: ХОРОШО. Приемлемая эффективность.\n";
            } else {
                report_file << "    - Оценка: ТРЕБУЕТ ОПТИМИЗАЦИИ. Низкая эффективность.\n";
            }
            report_file << "\n";
        }
    }
    
    // Анализ для средних количеств потоков (7-8)
    report_file << "ГРУППА 2: Среднее количество потоков (7-8)\n";
    report_file << "--------------------------------------------------------------------------------\n";
    for (const auto& res : results) {
        if (res.num_threads >= 7 && res.num_threads <= 8) {
            report_file << "  " << res.num_threads << " поток(а):\n";
            report_file << "    - Время выполнения: " << std::fixed << std::setprecision(4) << res.total_time << " сек\n";
            report_file << "    - Ускорение: " << std::setprecision(2) << res.speedup << "x\n";
            report_file << "    - Эффективность: " << std::setprecision(1) << res.efficiency << "%\n";
            if (res.efficiency > 80) {
                report_file << "    - Оценка: ОТЛИЧНО! Хорошая масштабируемость.\n";
            } else if (res.efficiency > 50) {
                report_file << "    - Оценка: УДОВЛЕТВОРИТЕЛЬНО. Заметны накладные расходы.\n";
            } else {
                report_file << "    - Оценка: ПЛОХО. Сильные накладные расходы на синхронизацию.\n";
            }
            report_file << "\n";
        }
    }
    
    // Анализ для больших количеств потоков (16-40)
    report_file << "ГРУППА 3: Большое количество потоков (16-40)\n";
    report_file << "--------------------------------------------------------------------------------\n";
    for (const auto& res : results) {
        if (res.num_threads >= 16) {
            report_file << "  " << res.num_threads << " поток(а):\n";
            report_file << "    - Время выполнения: " << std::fixed << std::setprecision(4) << res.total_time << " сек\n";
            report_file << "    - Ускорение: " << std::setprecision(2) << res.speedup << "x\n";
            report_file << "    - Эффективность: " << std::setprecision(1) << res.efficiency << "%\n";
            if (res.efficiency > 60) {
                report_file << "    - Оценка: ХОРОШО для такого количества потоков.\n";
            } else if (res.efficiency > 30) {
                report_file << "    - Оценка: УДОВЛЕТВОРИТЕЛЬНО. Ограничения аппаратных ресурсов.\n";
            } else {
                report_file << "    - Оценка: ПЛОХО. Переключение контекста и contention.\n";
            }
            report_file << "\n";
        }
    }
    
    report_file << "================================================================================\n";
    report_file << "                              ОБЩИЙ ВЫВОД\n";
    report_file << "================================================================================\n\n";
    
    // Находим оптимальное количество потоков
    auto best_it = std::max_element(results.begin(), results.end(), 
                                    [](const TestResult& a, const TestResult& b) {
                                        return a.speedup < b.speedup;
                                    });
    
    report_file << "1. ОПТИМАЛЬНОЕ КОЛИЧЕСТВО ПОТОКОВ:\n";
    report_file << "   Лучшее ускорение " << std::fixed << std::setprecision(2) << best_it->speedup 
                << "x достигнуто при " << best_it->num_threads << " потоках.\n";
    report_file << "   Эффективность: " << std::setprecision(1) << best_it->efficiency << "%\n\n";
    
    report_file << "2. МАСШТАБИРУЕМОСТЬ:\n";
    // Анализ тренда
    bool linear_scaling = true;
    for (size_t i = 1; i < results.size() && results[i].num_threads <= 8; ++i) {
        double expected_speedup = results[i].num_threads;
        double actual_ratio = results[i].speedup / results[0].speedup * results[0].num_threads;
        if (actual_ratio < expected_speedup * 0.7) {
            linear_scaling = false;
            break;
        }
    }
    
    if (linear_scaling) {
        report_file << "   - На диапазоне 1-8 потоков наблюдается БЛИЗКОЕ К ЛИНЕЙНОМУ ускорение.\n";
        report_file << "   - Программа эффективно использует дополнительные ядра.\n";
    } else {
        report_file << "   - Наблюдаются ОТКЛОНЕНИЯ от линейного ускорения уже на малом количестве потоков.\n";
        report_file << "   - Возможные причины: накладные расходы на создание потоков, синхронизацию.\n";
    }
    
    report_file << "\n3. ПРЕДЕЛЫ МАСШТАБИРУЕМОСТИ:\n";
    if (best_it->num_threads < 40) {
        report_file << "   - После " << best_it->num_threads << " потоков эффективность СНИЖАЕТСЯ.\n";
        report_file << "   - Причины: переключение контекста, contention за ресурсы памяти, ограничения кэша.\n";
    } else {
        report_file << "   - Ускорение продолжает расти до 40 потоков.\n";
        report_file << "   - Рекомендуется тестирование на большем количестве ядер.\n";
    }
    
    report_file << "\n4. РЕКОМЕНДАЦИИ:\n";
    report_file << "   - Использовать " << best_it->num_threads << " потоков для данной задачи и размера матрицы.\n";
    report_file << "   - Для больших матриц можно увеличить количество потоков.\n";
    report_file << "   - Рассмотреть использование thread pool для уменьшения накладных расходов.\n";
    report_file << "   - Оптимизировать доступ к памяти (локальность данных).\n\n";
    
    report_file << "================================================================================\n";
    report_file << "                         ДАННЫЕ ДЛЯ ПОСТРОЕНИЯ ГРАФИКА\n";
    report_file << "================================================================================\n";
    report_file << "Файл с данными: report_data.csv\n";
    report_file << "Скрипт для построения: plot_script.gnuplot\n";
    report_file << "Команда для построения графика: gnuplot plot_script.gnuplot\n\n";
    
    report_file << "Для создания PDF отчета выполните:\n";
    report_file << "  1. gnuplot plot_script.gnuplot  (создаст speedup_chart.png)\n";
    report_file << "  2. convert detailed_report.txt speedup_chart.png report.pdf\n";
    report_file << "     или используйте pandoc: pandoc detailed_report.txt --pdf-engine=wkhtmltopdf -o report.pdf\n";
    report_file << "================================================================================\n";
    
    report_file.close();
    std::cout << "Создан подробный отчет: detailed_report.txt" << std::endl;
    
    // Попытка построить график если есть gnuplot
    std::cout << "\nПопытка построения графика..." << std::endl;
    int ret = system("gnuplot plot_script.gnuplot 2>/dev/null");
    if (ret == 0) {
        std::cout << "График успешно создан: speedup_chart.png" << std::endl;
        
        // Попытка создать PDF
        std::cout << "Попытка создания PDF..." << std::endl;
        ret = system("which convert >/dev/null 2>&1 && convert detailed_report.txt speedup_chart.png report.pdf || echo 'ImageMagick не найден'");
        if (ret != 0) {
            std::cout << "Не удалось создать PDF автоматически (требуется ImageMagick или pandoc)." << std::endl;
            std::cout << "Вручную объедините detailed_report.txt и speedup_chart.png в PDF." << std::endl;
        } else {
            std::cout << "PDF отчет создан: report.pdf" << std::endl;
        }
    } else {
        std::cout << "gnuplot не найден. График не построен." << std::endl;
        std::cout << "Используйте данные из report_data.csv для построения графика вручную." << std::endl;
    }
    
    std::cout << "\n=== Тестирование завершено ===" << std::endl;
    std::cout << "Созданные файлы:" << std::endl;
    std::cout << "  - report_data.csv (данные)" << std::endl;
    std::cout << "  - plot_script.gnuplot (скрипт для графика)" << std::endl;
    std::cout << "  - detailed_report.txt (подробный текстовый отчет)" << std::endl;
    std::cout << "  - speedup_chart.png (график, если установлен gnuplot)" << std::endl;
    std::cout << "  - report.pdf (если удалось создать автоматически)" << std::endl;
    
    return 0;
}
