#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <fstream>
#include <cmath>
#include <random>
#include <chrono>
#include <iomanip>
#include <string>
#include <sstream>

// Шаблон класса сервера для обработки задач
template<typename T>
class TaskServer {
public:
    using TaskFunc = std::function<T()>;
    
    TaskServer() : stop_flag(false), task_id_counter(0) {}
    
    // Запустить сервер
    void start() {
        stop_flag = false;
        server_thread = std::jthread([this](std::stop_token stoken) {
            run_server(stoken);
        });
        std::cout << "Сервер запущен" << std::endl;
    }
    
    // Остановить сервер
    void stop() {
        stop_flag = true;
        cond_var.notify_all();
        if (server_thread.joinable()) {
            server_thread.request_stop();
            server_thread.join();
        }
        std::cout << "Сервер остановлен" << std::endl;
    }
    
    // Добавить задачу, возвращает ID задачи
    size_t add_task(TaskFunc task) {
        std::promise<T> promise;
        T result = task();
        promise.set_value(result);
        std::future<T> future = promise.get_future();
        
        size_t task_id;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_id = ++task_id_counter;
            // Сохраняем результат напрямую
            std::lock_guard<std::mutex> res_lock(results_mutex);
            results[task_id] = result;
        }
        results_cond.notify_all();
        
        return task_id;
    }
    
    // Получить результат (блокирующий)
    T request_result(size_t task_id) {
        std::unique_lock<std::mutex> lock(results_mutex);
        results_cond.wait(lock, [this, task_id] {
            return results.find(task_id) != results.end();
        });
        
        T result = std::move(results[task_id]);
        results.erase(task_id);
        return result;
    }
    
    // Проверить наличие результата (неблокирующий)
    bool has_result(size_t task_id) {
        std::lock_guard<std::mutex> lock(results_mutex);
        return results.find(task_id) != results.end();
    }

private:
    void run_server(std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            cond_var.wait(lock, [this, &stoken] {
                return !task_queue.empty() || stoken.stop_requested();
            });
            
            if (stoken.stop_requested() && task_queue.empty()) {
                break;
            }
            
            if (!task_queue.empty()) {
                auto task_pair = std::move(task_queue.front());
                size_t task_id = task_pair.first;
                auto task_func = std::move(task_pair.second);
                task_queue.pop();
                lock.unlock();
                
                try {
                    T result = task_func();
                    {
                        std::lock_guard<std::mutex> res_lock(results_mutex);
                        results[task_id] = std::move(result);
                    }
                    results_cond.notify_all();
                } catch (const std::exception& e) {
                    std::cerr << "Ошибка выполнения задачи " << task_id << ": " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Ошибка выполнения задачи " << task_id << std::endl;
                }
            }
        }
    }
    
    std::queue<std::pair<size_t, std::function<T()>>> task_queue;
    std::unordered_map<size_t, T> results;
    
    std::mutex queue_mutex;
    std::mutex results_mutex;
    std::condition_variable cond_var;
    std::condition_variable results_cond;
    
    std::jthread server_thread;
    std::atomic<bool> stop_flag;
    std::atomic<size_t> task_id_counter;
};

// Классы задач
struct SinTask {
    double arg;
    double operator()() const { return std::sin(arg); }
};

struct SqrtTask {
    double arg;
    double operator()() const { return std::sqrt(arg); }
};

struct PowTask {
    double base;
    double exp;
    double operator()() const { return std::pow(base, exp); }
};

// Клиентский поток
template<typename TaskType>
void client_thread(TaskServer<double>& server, int num_tasks, 
                   const std::string& client_name, const std::string& output_file) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist_sin(-10.0, 10.0);
    std::uniform_real_distribution<> dist_sqrt(0.0, 100.0);
    std::uniform_real_distribution<> dist_base(1.0, 10.0);
    std::uniform_real_distribution<> dist_exp(0.0, 5.0);
    
    std::vector<std::pair<size_t, double>> tasks_results;
    std::vector<double> expected_results;
    
    std::cout << "[" << client_name << "] Начало работы, задач: " << num_tasks << std::endl;
    
    for (int i = 0; i < num_tasks; ++i) {
        TaskType task;
        double expected;
        
        if constexpr (std::is_same_v<TaskType, SinTask>) {
            task.arg = dist_sin(gen);
            expected = std::sin(task.arg);
        } else if constexpr (std::is_same_v<TaskType, SqrtTask>) {
            task.arg = dist_sqrt(gen);
            expected = std::sqrt(task.arg);
        } else if constexpr (std::is_same_v<TaskType, PowTask>) {
            task.base = dist_base(gen);
            task.exp = dist_exp(gen);
            expected = std::pow(task.base, task.exp);
        }
        
        expected_results.push_back(expected);
        size_t task_id = server.add_task([task]() { return task(); });
        tasks_results.push_back({task_id, expected});
    }
    
    // Получение результатов и запись в файл
    std::ofstream out(output_file);
    out << "# Результаты клиента: " << client_name << "\n";
    out << "# Формат: ID_задачи | Аргументы | Результат сервера | Ожидаемый результат | Статус\n";
    out << "#================================================================================\n\n";
    
    int success_count = 0;
    for (size_t i = 0; i < tasks_results.size(); ++i) {
        size_t task_id = tasks_results[i].first;
        double expected = tasks_results[i].second;
        
        double result = server.request_result(task_id);
        
        bool is_correct = std::abs(result - expected) < 1e-9;
        if (is_correct) success_count++;
        
        std::string status = is_correct ? "OK" : "FAIL";
        
        out << std::setw(6) << task_id << " | ";
        if constexpr (std::is_same_v<TaskType, SinTask>) {
            out << std::fixed << std::setprecision(6) << "sin(" << expected_results[i] << ")";
        } else if constexpr (std::is_same_v<TaskType, SqrtTask>) {
            out << std::fixed << std::setprecision(6) << "sqrt(" << expected_results[i] << ")";
        } else if constexpr (std::is_same_v<TaskType, PowTask>) {
            // Для pow нужно сохранить аргументы отдельно
            out << std::fixed << std::setprecision(6) << "pow(...)";
        }
        out << " | " << std::setw(15) << std::setprecision(10) << result 
            << " | " << std::setw(15) << expected 
            << " | " << status << "\n";
    }
    
    out << "\n#================================================================================\n";
    out << "# Итого: " << success_count << " из " << num_tasks << " задач выполнено корректно\n";
    out << "# Процент успеха: " << std::fixed << std::setprecision(1) 
        << (100.0 * success_count / num_tasks) << "%\n";
    
    out.close();
    std::cout << "[" << client_name << "] Завершено. Успешно: " << success_count 
              << "/" << num_tasks << ". Результаты в файле: " << output_file << std::endl;
}

// Тест для проверки результатов
bool test_results(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    int total_tasks = 0;
    int success_tasks = 0;
    
    while (std::getline(file, line)) {
        if (line.find("| OK") != std::string::npos) {
            success_tasks++;
            total_tasks++;
        } else if (line.find("| FAIL") != std::string::npos) {
            total_tasks++;
        }
    }
    
    file.close();
    
    std::cout << "Тест файла " << filename << ": " << success_tasks 
              << "/" << total_tasks << " задач пройдено" << std::endl;
    
    return success_tasks == total_tasks && total_tasks > 0;
}

int main(int argc, char* argv[]) {
    int num_tasks = 100; // Количество задач по умолчанию
    if (argc > 1) {
        num_tasks = std::atoi(argv[1]);
        if (num_tasks < 5 || num_tasks > 10000) {
            std::cerr << "Количество задач должно быть в диапазоне 5-10000" << std::endl;
            return 1;
        }
    }
    
    std::cout << "=== Клиент-серверное приложение ===" << std::endl;
    std::cout << "Количество задач на клиента: " << num_tasks << std::endl;
    std::cout << std::endl;
    
    // Создание сервера
    TaskServer<double> server;
    
    // Запуск сервера
    server.start();
    
    // Создание клиентских потоков
    std::thread client1(client_thread<SinTask>, std::ref(server), num_tasks, 
                       "Client-Sin", "results_sin.txt");
    std::thread client2(client_thread<SqrtTask>, std::ref(server), num_tasks, 
                       "Client-Sqrt", "results_sqrt.txt");
    std::thread client3(client_thread<PowTask>, std::ref(server), num_tasks, 
                       "Client-Pow", "results_pow.txt");
    
    // Ожидание завершения клиентов
    client1.join();
    client2.join();
    client3.join();
    
    // Остановка сервера
    server.stop();
    
    std::cout << "\n=== Тестирование результатов ===" << std::endl;
    
    bool all_passed = true;
    all_passed &= test_results("results_sin.txt");
    all_passed &= test_results("results_sqrt.txt");
    all_passed &= test_results("results_pow.txt");
    
    std::cout << "\n=== Общий результат ===" << std::endl;
    if (all_passed) {
        std::cout << "ВСЕ ТЕСТЫ ПРОЙДЕНЫ УСПЕШНО!" << std::endl;
    } else {
        std::cout << "НЕКОТОРЫЕ ТЕСТЫ НЕ ПРОЙДЕНЫ!" << std::endl;
        return 1;
    }
    
    return 0;
}
