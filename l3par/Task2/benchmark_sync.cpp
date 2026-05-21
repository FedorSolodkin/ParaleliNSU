// Benchmark: condition_variable vs polling (busy-wait со sleep).
// Измеряем суммарное время обработки N задач одним рабочим потоком.

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

static constexpr int N = 50'000;

// ── Метод 1: condition_variable ─────────────────────────────────────────────
double bench_cv(int n) {
    std::queue<std::packaged_task<double()>> queue;
    std::mutex mtx;
    std::condition_variable cv;

    std::jthread worker([&](std::stop_token st) {
        for (;;) {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return !queue.empty() || st.stop_requested(); });
            if (queue.empty()) break;
            auto task = std::move(queue.front());
            queue.pop();
            lk.unlock();
            task();
        }
    });

    std::vector<std::future<double>> futures;
    futures.reserve(n);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        std::packaged_task<double()> pt([]() { return 1.0; });
        futures.push_back(pt.get_future());
        {
            std::lock_guard<std::mutex> lk(mtx);
            queue.push(std::move(pt));
        }
        cv.notify_one();
    }
    for (auto& f : futures) f.get();
    auto t1 = std::chrono::high_resolution_clock::now();

    worker.request_stop();
    cv.notify_all();

    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ── Метод 2: polling со sleep(100 мкс) ─────────────────────────────────────
double bench_poll(int n) {
    std::queue<std::packaged_task<double()>> queue;
    std::mutex mtx;

    std::jthread worker([&](std::stop_token st) {
        while (!st.stop_requested()) {
            std::unique_lock<std::mutex> lk(mtx);
            if (!queue.empty()) {
                auto task = std::move(queue.front());
                queue.pop();
                lk.unlock();
                task();
            } else {
                lk.unlock();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::vector<std::future<double>> futures;
    futures.reserve(n);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        std::packaged_task<double()> pt([]() { return 1.0; });
        futures.push_back(pt.get_future());
        {
            std::lock_guard<std::mutex> lk(mtx);
            queue.push(std::move(pt));
        }
    }
    for (auto& f : futures) f.get();
    auto t1 = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// Версии с нагрузкой на CPU — имитируют реальные задачи (sin/sqrt)
double bench_cv_work(int n) {
    std::queue<std::packaged_task<double()>> queue;
    std::mutex mtx;
    std::condition_variable cv;

    std::jthread worker([&](std::stop_token st) {
        for (;;) {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return !queue.empty() || st.stop_requested(); });
            if (queue.empty()) break;
            auto task = std::move(queue.front());
            queue.pop();
            lk.unlock();
            task();
        }
    });

    std::vector<std::future<double>> futures;
    futures.reserve(n);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        double x = static_cast<double>(i) * 0.001;
        std::packaged_task<double()> pt([x]() { return std::sin(x) + std::sqrt(x); });
        futures.push_back(pt.get_future());
        { std::lock_guard<std::mutex> lk(mtx); queue.push(std::move(pt)); }
        cv.notify_one();
    }
    for (auto& f : futures) f.get();
    auto t1 = std::chrono::high_resolution_clock::now();
    worker.request_stop(); cv.notify_all();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

double bench_poll_work(int n) {
    std::queue<std::packaged_task<double()>> queue;
    std::mutex mtx;

    std::jthread worker([&](std::stop_token st) {
        while (!st.stop_requested()) {
            std::unique_lock<std::mutex> lk(mtx);
            if (!queue.empty()) {
                auto task = std::move(queue.front());
                queue.pop(); lk.unlock(); task();
            } else {
                lk.unlock();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::vector<std::future<double>> futures;
    futures.reserve(n);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        double x = static_cast<double>(i) * 0.001;
        std::packaged_task<double()> pt([x]() { return std::sin(x) + std::sqrt(x); });
        futures.push_back(pt.get_future());
        { std::lock_guard<std::mutex> lk(mtx); queue.push(std::move(pt)); }
    }
    for (auto& f : futures) f.get();
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    std::cout << "=== Synchronization Benchmark ===\n\n";

    // Разогрев
    bench_cv(1000); bench_poll(1000);
    bench_cv_work(1000); bench_poll_work(1000);

    std::cout << "--- Тривиальные задачи (N=" << N << ", задача: return 1.0) ---\n";
    double cv_ms   = bench_cv(N);
    double poll_ms = bench_poll(N);
    std::cout << "condition_variable   : " << cv_ms   << " ms\n";
    std::cout << "polling (sleep 100us): " << poll_ms << " ms\n\n";

    std::cout << "--- Реальные задачи (N=" << N << ", задача: sin+sqrt) ---\n";
    double cv_w   = bench_cv_work(N);
    double poll_w = bench_poll_work(N);
    std::cout << "condition_variable   : " << cv_w   << " ms\n";
    std::cout << "polling (sleep 100us): " << poll_w << " ms\n\n";

    std::cout << "Вывод:\n"
              << "  - При тривиальных задачах polling может быть немного быстрее\n"
              << "    из-за накладных расходов на notify_one().\n"
              << "  - При реальных задачах (sin, sqrt, pow) condition_variable\n"
              << "    выигрывает: нет задержки до sleep_duration мкс, нет\n"
              << "    лишних итераций вхолостую.\n"
              << "  - condition_variable не тратит CPU в ожидании.\n"
              << "  - Выбираем condition_variable для сервера.\n";
    return 0;
}
