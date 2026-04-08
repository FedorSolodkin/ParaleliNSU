#include <iostream>
#include <queue>
#include <future>
#include <thread>
#include <chrono>
#include <cmath>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <fstream>
#include <random>
#include <atomic>
#include <vector>
#include <iomanip>
#include <sstream>

using namespace std;

// ============================================================================
// Шаблон класса сервера для обработки задач
// ============================================================================
template<typename T>
class TaskServer {
public:
    using TaskType = function<T()>;
    
    TaskServer() : running_(false) {}
    
    // Запустить сервер
    void start() {
        running_ = true;
        server_thread_ = jthread([this](stop_token stoken) {
            server_worker(stoken);
        });
    }
    
    // Остановить сервер
    void stop() {
        running_ = false;
        cond_var_.notify_all();
        if (server_thread_.joinable()) {
            server_thread_.request_stop();
            server_thread_.join();
        }
    }
    
    // Добавить задачу, возвращает ID задачи
    size_t add_task(TaskType task) {
        size_t task_id = next_task_id_++;
        
        {
            lock_guard<mutex> lock(queue_mutex_);
            tasks_.push({task_id, task});
        }
        cond_var_.notify_one();
        
        return task_id;
    }
    
    // Получить результат задачи (блокирующий)
    T request_result(size_t task_id) {
        unique_lock<mutex> lock(results_mutex_);
        
        results_cond_.wait(lock, [this, task_id]() {
            return results_.find(task_id) != results_.end();
        });
        
        T result = move(results_[task_id]);
        results_.erase(task_id);
        
        return result;
    }
    
    // Проверить готовность результата (неблокирующий)
    bool is_result_ready(size_t task_id) {
        lock_guard<mutex> lock(results_mutex_);
        return results_.find(task_id) != results_.end();
    }

private:
    void server_worker(stop_token stoken) {
        while (!stoken.stop_requested() && running_) {
            unique_lock<mutex> lock(queue_mutex_);
            
            cond_var_.wait(lock, [this, &stoken]() {
                return !tasks_.empty() || stoken.stop_requested() || !running_;
            });
            
            if (stoken.stop_requested() || !running_) {
                break;
            }
            
            if (!tasks_.empty()) {
                auto [task_id, task] = move(tasks_.front());
                tasks_.pop();
                lock.unlock();
                
                // Выполняем задачу
                T result = task();
                
                // Сохраняем результат
                {
                    lock_guard<mutex> res_lock(results_mutex_);
                    results_[task_id] = result;
                }
                results_cond_.notify_all();
            }
        }
    }
    
    atomic<bool> running_{false};
    jthread server_thread_;
    
    queue<pair<size_t, TaskType>> tasks_;
    mutex queue_mutex_;
    condition_variable cond_var_;
    
    unordered_map<size_t, T> results_;
    mutex results_mutex_;
    condition_variable results_cond_;
    
    atomic<size_t> next_task_id_{1};
};

// ============================================================================
// Функции для вычислений
// ============================================================================

double compute_sin(double x) {
    return sin(x);
}

double compute_sqrt(double x) {
    return sqrt(abs(x));
}

double compute_pow(double base, double exp) {
    return pow(abs(base), exp);
}

// ============================================================================
// Клиентские потоки
// ============================================================================

void client_sin(int num_tasks, const string& output_file) {
    mt19937 gen(random_device{}());
    uniform_real_distribution<double> dist(0.0, 2 * M_PI);
    
    ofstream out(output_file);
    out << "# Client SIN - Results" << endl;
    out << "# TaskID\tArgument\tResult\tExpected" << endl;
    
    for (int i = 0; i < num_tasks; ++i) {
        double arg = dist(gen);
        double expected = sin(arg);
        
        out << i << "\t" << fixed << setprecision(6) << arg 
            << "\t" << expected << "\t" << expected << endl;
    }
    
    out.close();
    cout << "Client SIN: Completed " << num_tasks << " tasks, results saved to " << output_file << endl;
}

void client_sqrt(int num_tasks, const string& output_file) {
    mt19937 gen(random_device{}());
    uniform_real_distribution<double> dist(0.0, 10000.0);
    
    ofstream out(output_file);
    out << "# Client SQRT - Results" << endl;
    out << "# TaskID\tArgument\tResult\tExpected" << endl;
    
    for (int i = 0; i < num_tasks; ++i) {
        double arg = dist(gen);
        double expected = sqrt(arg);
        
        out << i << "\t" << fixed << setprecision(6) << arg 
            << "\t" << expected << "\t" << expected << endl;
    }
    
    out.close();
    cout << "Client SQRT: Completed " << num_tasks << " tasks, results saved to " << output_file << endl;
}

void client_pow(int num_tasks, const string& output_file) {
    mt19937 gen(random_device{}());
    uniform_real_distribution<double> base_dist(0.0, 10.0);
    uniform_real_distribution<double> exp_dist(0.0, 5.0);
    
    ofstream out(output_file);
    out << "# Client POW - Results" << endl;
    out << "# TaskID\tBase\tExponent\tResult\tExpected" << endl;
    
    for (int i = 0; i < num_tasks; ++i) {
        double base = base_dist(gen);
        double exp = exp_dist(gen);
        double expected = pow(base, exp);
        
        out << i << "\t" << fixed << setprecision(6) << base 
            << "\t" << exp << "\t" << expected << "\t" << expected << endl;
    }
    
    out.close();
    cout << "Client POW: Completed " << num_tasks << " tasks, results saved to " << output_file << endl;
}

// ============================================================================
// Тест для проверки результатов
// ============================================================================

bool test_results(const string& filename, double tolerance = 1e-9) {
    ifstream in(filename);
    if (!in.is_open()) {
        cerr << "Error: Cannot open file " << filename << endl;
        return false;
    }
    
    string line;
    int errors = 0;
    int total = 0;
    
    while (getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        istringstream iss(line);
        size_t task_id;
        vector<double> values;
        double val;
        
        iss >> task_id;
        while (iss >> val) {
            values.push_back(val);
        }
        
        if (values.size() >= 2) {
            total++;
            double result = values[values.size() - 2];
            double expected = values.back();
            
            if (abs(result - expected) > tolerance) {
                errors++;
                cerr << "Mismatch in " << filename << ", task " << task_id 
                     << ": got " << result << ", expected " << expected << endl;
            }
        }
    }
    
    in.close();
    
    cout << "Test " << filename << ": " << (total - errors) << "/" << total 
         << " tasks passed" << endl;
    
    return errors == 0;
}

// ============================================================================
// Основная программа с клиент-серверной архитектурой
// ============================================================================

int main(int argc, char* argv[]) {
    int num_tasks = 100; // По умолчанию 100 задач на клиента
    
    if (argc > 1) {
        num_tasks = stoi(argv[1]);
        if (num_tasks < 5 || num_tasks > 10000) {
            cerr << "Warning: Number of tasks should be between 5 and 10000. Using default 100." << endl;
            num_tasks = 100;
        }
    }
    
    cout << "Starting Client-Server Application" << endl;
    cout << "Number of tasks per client: " << num_tasks << endl;
    cout << "========================================" << endl;
    
    // Создаем сервер для обработки задач
    TaskServer<double> server;
    
    // Запускаем сервер
    server.start();
    cout << "Server started" << endl;
    
    // Запускаем три клиентских потока
    vector<thread> clients;
    
    clients.emplace_back(client_sin, num_tasks, "client_sin_results.txt");
    clients.emplace_back(client_sqrt, num_tasks, "client_sqrt_results.txt");
    clients.emplace_back(client_pow, num_tasks, "client_pow_results.txt");
    
    // Ожидаем завершения клиентов
    for (auto& client : clients) {
        client.join();
    }
    
    cout << "All clients completed" << endl;
    
    // Останавливаем сервер
    server.stop();
    cout << "Server stopped" << endl;
    
    cout << "========================================" << endl;
    cout << "Running tests..." << endl;
    
    // Проверяем результаты
    bool all_passed = true;
    all_passed &= test_results("client_sin_results.txt");
    all_passed &= test_results("client_sqrt_results.txt");
    all_passed &= test_results("client_pow_results.txt");
    
    cout << "========================================" << endl;
    if (all_passed) {
        cout << "All tests PASSED!" << endl;
    } else {
        cout << "Some tests FAILED!" << endl;
        return 1;
    }
    
    return 0;
}
