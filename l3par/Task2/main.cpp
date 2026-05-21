#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "server.hpp"
#include "thread_pool_server.hpp"

// Количество задач на клиента (условие: 5 < N < 10000)
static constexpr int TASKS_PER_CLIENT = 10000;

// ── Клиент 1: вычисление sin(x), аргумент x ∈ [0, 2π] ──────────────────────
// Клиент работает в отдельном потоке. Сначала отправляет все TASKS_PER_CLIENT
// задач на сервер (получая id для каждой), затем забирает результаты и пишет в файл.
template<typename ServerT>
void client_sin(ServerT& srv, const std::string& output_file) {
    std::mt19937_64 gen(42);
    std::uniform_real_distribution<double> rnd(0.0, 2.0 * std::numbers::pi);

    // pending хранит пары (аргумент, id задачи) — нужно для записи в файл
    std::vector<std::pair<double, size_t>> pending;
    pending.reserve(TASKS_PER_CLIENT);

    // Фаза 1: добавляем все задачи. Аргумент захватывается по значению в лямбду.
    for (int i = 0; i < TASKS_PER_CLIENT; ++i) {
        double input_val = rnd(gen);
        size_t task_id   = srv.add_task([input_val]() -> double {
            return std::sin(input_val);
        });
        pending.emplace_back(input_val, task_id);
    }

    // Фаза 2: забираем результаты и пишем в файл
    std::ofstream ofs(output_file);
    ofs << "# Тип задачи: SIN\n# Формат: аргумент результат\n";
    ofs << std::fixed << std::setprecision(15);
    for (auto& [input_val, task_id] : pending)
        ofs << input_val << " " << srv.request_result(task_id) << "\n";

    std::cout << "[SIN]  " << TASKS_PER_CLIENT << " задач -> " << output_file << "\n";
}

// ── Клиент 2: вычисление sqrt(x), аргумент x ∈ [0, 10000] ──────────────────
template<typename ServerT>
void client_sqrt(ServerT& srv, const std::string& output_file) {
    std::mt19937_64 gen(123);
    std::uniform_real_distribution<double> rnd(0.0, 10000.0);

    std::vector<std::pair<double, size_t>> pending;
    pending.reserve(TASKS_PER_CLIENT);

    for (int i = 0; i < TASKS_PER_CLIENT; ++i) {
        double input_val = rnd(gen);
        size_t task_id   = srv.add_task([input_val]() -> double {
            return std::sqrt(input_val);
        });
        pending.emplace_back(input_val, task_id);
    }

    std::ofstream ofs(output_file);
    ofs << "# Тип задачи: SQRT\n# Формат: аргумент результат\n";
    ofs << std::fixed << std::setprecision(15);
    for (auto& [input_val, task_id] : pending)
        ofs << input_val << " " << srv.request_result(task_id) << "\n";

    std::cout << "[SQRT] " << TASKS_PER_CLIENT << " задач -> " << output_file << "\n";
}

// ── Клиент 3: вычисление pow(base, exp), оба аргумента рандомные ─────────────
template<typename ServerT>
void client_pow(ServerT& srv, const std::string& output_file) {
    std::mt19937_64 gen(456);
    std::uniform_real_distribution<double> base_rnd(0.1, 10.0);
    std::uniform_real_distribution<double> exp_rnd(0.1, 5.0);

    // Храним (base, exponent, task_id) — три поля нужны для записи в файл
    std::vector<std::tuple<double, double, size_t>> pending;
    pending.reserve(TASKS_PER_CLIENT);

    for (int i = 0; i < TASKS_PER_CLIENT; ++i) {
        double base_val = base_rnd(gen);
        double exp_val  = exp_rnd(gen);
        size_t task_id  = srv.add_task([base_val, exp_val]() -> double {
            return std::pow(base_val, exp_val);
        });
        pending.emplace_back(base_val, exp_val, task_id);
    }

    std::ofstream ofs(output_file);
    ofs << "# Тип задачи: POW\n# Формат: основание показатель результат\n";
    ofs << std::fixed << std::setprecision(15);
    for (auto& [base_val, exp_val, task_id] : pending)
        ofs << base_val << " " << exp_val << " " << srv.request_result(task_id) << "\n";

    std::cout << "[POW]  " << TASKS_PER_CLIENT << " задач -> " << output_file << "\n";
}

// ── Запуск трёх клиентов одновременно (каждый в отдельном потоке) ──────────
template<typename ServerT>
void run_demo(ServerT& server, const std::string& file_prefix) {
    server.start();

    auto wall_start = std::chrono::steady_clock::now();

    // Три потока-клиента работают параллельно — каждый добавляет свои задачи
    std::thread th_sin ([&]() { client_sin (server, file_prefix + "results_sin.txt");  });
    std::thread th_sqrt([&]() { client_sqrt(server, file_prefix + "results_sqrt.txt"); });
    std::thread th_pow ([&]() { client_pow (server, file_prefix + "results_pow.txt");  });
    th_sin.join(); th_sqrt.join(); th_pow.join();

    auto wall_end = std::chrono::steady_clock::now();
    server.stop();

    double elapsed_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();
    std::cout << "Суммарное время: " << elapsed_ms << " мс\n\n";
}

int main() {
    std::cout << "=== Однопоточный сервер ===\n";
    {
        Server<double> srv;
        run_demo(srv, "");
    }

    std::cout << "=== Сервер с пулом потоков (4 воркера) ===\n";
    {
        ThreadPoolServer<double> pool_srv(4);
        run_demo(pool_srv, "pool_");
    }

    return 0;
}
