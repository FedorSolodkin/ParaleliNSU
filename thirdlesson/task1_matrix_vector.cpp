#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>

using namespace std;

// Функция для инициализации массива (параллельная)
void parallel_init(vector<double>& arr, size_t start, size_t end, unsigned int seed) {
    mt19937 gen(seed);
    uniform_real_distribution<double> dist(0.0, 1.0);
    for (size_t i = start; i < end; ++i) {
        arr[i] = dist(gen);
    }
}

// Функция умножения матрицы на вектор (параллельная версия)
void matrix_vector_multiply(const vector<vector<double>>& matrix, 
                            const vector<double>& vec, 
                            vector<double>& result,
                            size_t start_row, size_t end_row) {
    for (size_t i = start_row; i < end_row; ++i) {
        result[i] = 0.0;
        for (size_t j = 0; j < vec.size(); ++j) {
            result[i] += matrix[i][j] * vec[j];
        }
    }
}

// Основная функция для выполнения теста с заданным количеством потоков
double run_test(size_t matrix_size, size_t num_threads) {
    // Выделение памяти
    vector<vector<double>> matrix(matrix_size, vector<double>(matrix_size));
    vector<double> vec(matrix_size);
    vector<double> result(matrix_size);
    
    auto start_total = chrono::high_resolution_clock::now();
    
    // Параллельная инициализация матрицы
    vector<thread> init_threads;
    size_t chunk_size = (matrix_size * matrix_size + num_threads - 1) / num_threads;
    
    for (size_t t = 0; t < num_threads; ++t) {
        size_t start_idx = t * chunk_size;
        size_t end_idx = min((t + 1) * chunk_size, matrix_size * matrix_size);
        
        if (start_idx >= matrix_size * matrix_size) break;
        
        // Преобразуем линейный индекс в двумерный
        size_t start_row = start_idx / matrix_size;
        size_t end_row = min((end_idx + matrix_size - 1) / matrix_size, matrix_size);
        
        init_threads.emplace_back([&matrix, start_row, end_row, matrix_size, t]() {
            // Плоское представление для инициализации
            for (size_t row = start_row; row < end_row; ++row) {
                mt19937 gen(static_cast<unsigned int>(t * 1000 + row));
                uniform_real_distribution<double> dist(0.0, 1.0);
                for (size_t col = 0; col < matrix_size; ++col) {
                    matrix[row][col] = dist(gen);
                }
            }
        });
    }
    
    // Параллельная инициализация вектора
    vector<thread> vec_init_threads;
    size_t vec_chunk = (matrix_size + num_threads - 1) / num_threads;
    
    for (size_t t = 0; t < num_threads; ++t) {
        size_t start_idx = t * vec_chunk;
        size_t end_idx = min((t + 1) * vec_chunk, matrix_size);
        
        if (start_idx >= matrix_size) break;
        
        vec_init_threads.emplace_back([&vec, start_idx, end_idx, t]() {
            mt19937 gen(static_cast<unsigned int>(t * 2000));
            uniform_real_distribution<double> dist(0.0, 1.0);
            for (size_t i = start_idx; i < end_idx; ++i) {
                vec[i] = dist(gen);
            }
        });
    }
    
    // Ожидание завершения инициализации
    for (auto& t : init_threads) t.join();
    for (auto& t : vec_init_threads) t.join();
    
    auto end_init = chrono::high_resolution_clock::now();
    double init_time = chrono::duration<double>(end_init - start_total).count();
    
    // Параллельное умножение матрицы на вектор
    vector<thread> compute_threads;
    size_t row_chunk = (matrix_size + num_threads - 1) / num_threads;
    
    auto start_compute = chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < num_threads; ++t) {
        size_t start_row = t * row_chunk;
        size_t end_row = min((t + 1) * row_chunk, matrix_size);
        
        if (start_row >= matrix_size) break;
        
        compute_threads.emplace_back([&matrix, &vec, &result, start_row, end_row]() {
            matrix_vector_multiply(matrix, vec, result, start_row, end_row);
        });
    }
    
    // Ожидание завершения вычислений
    for (auto& t : compute_threads) t.join();
    
    auto end_compute = chrono::high_resolution_clock::now();
    double compute_time = chrono::duration<double>(end_compute - start_compute).count();
    double total_time = chrono::duration<double>(end_compute - start_total).count();
    
    cout << "Threads: " << num_threads 
         << ", Init time: " << fixed << setprecision(4) << init_time << "s"
         << ", Compute time: " << compute_time << "s"
         << ", Total time: " << total_time << "s" << endl;
    
    return total_time;
}

int main(int argc, char* argv[]) {
    size_t matrix_size = 20000; // По умолчанию 20000x20000
    
    if (argc > 1) {
        matrix_size = stoul(argv[1]);
    }
    
    cout << "Matrix size: " << matrix_size << "x" << matrix_size << endl;
    cout << "========================================" << endl;
    
    // Количество потоков для тестирования
    vector<size_t> thread_counts = {1, 2, 4, 7, 8, 16, 20, 40};
    
    cout << "Threads\tTotal Time(s)\tSpeedup" << endl;
    cout << "----------------------------------------" << endl;
    
    double baseline_time = 0.0;
    
    for (size_t num_threads : thread_counts) {
        // Ограничиваем количество потоков реальным количеством ядер
        size_t actual_threads = min(num_threads, static_cast<size_t>(thread::hardware_concurrency()));
        if (actual_threads == 0) actual_threads = num_threads; // Если hardware_concurrency() возвращает 0
        
        double time = run_test(matrix_size, actual_threads);
        
        if (baseline_time == 0.0) {
            baseline_time = time;
            cout << actual_threads << "\t" << fixed << setprecision(4) << time << "\t" << "1.00" << endl;
        } else {
            double speedup = baseline_time / time;
            cout << actual_threads << "\t" << fixed << setprecision(4) << time << "\t" << setprecision(2) << speedup << endl;
        }
    }
    
    cout << "========================================" << endl;
    cout << "Hardware concurrency: " << thread::hardware_concurrency() << " threads" << endl;
    
    return 0;
}
