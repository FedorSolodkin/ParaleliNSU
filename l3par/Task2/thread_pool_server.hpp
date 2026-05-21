#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// Сервер с пулом потоков (Thread Pool).
//
// Отличие от Server<T>: вместо одного рабочего потока здесь pool_size_ потоков,
// которые одновременно тащат задачи из общей очереди. Это даёт ускорение,
// если задачи CPU-интенсивные и их много.
//
// Синхронизация такая же: condition_variable + два мьютекса (очередь и хранилище).
// Каждый рабочий поток самостоятельно берёт задачу и выполняет её.
template<typename T>
class ThreadPoolServer {
public:
    // По умолчанию создаём столько потоков, сколько аппаратных ядер
    explicit ThreadPoolServer(size_t pool_size = std::thread::hardware_concurrency())
        : pool_size_(pool_size) {}

    ~ThreadPoolServer() {
        if (active_.load()) stop();
    }

    void start() {
        active_ = true;
        pool_.reserve(pool_size_);
        for (size_t i = 0; i < pool_size_; ++i)
            pool_.emplace_back([this](std::stop_token token) { worker_loop(token); });
    }

    // Останавливаем все потоки сразу: каждый увидит stop_token и выйдет
    void stop() {
        if (!active_.exchange(false)) return;
        for (auto& worker : pool_) worker.request_stop();
        wake_cv_.notify_all(); // будим всех, чтобы увидели флаг остановки
    }

    // Интерфейс идентичен Server<T>
    size_t add_task(std::function<T()> func) {
        std::packaged_task<T()> ptask(std::move(func));
        std::shared_future<T> sfut = ptask.get_future().share();
        const size_t task_id = id_counter_++;

        {
            std::lock_guard<std::mutex> lk(store_mutex_);
            result_store_.emplace(task_id, sfut);
        }
        {
            std::lock_guard<std::mutex> lk(queue_mutex_);
            task_queue_.emplace(task_id, std::move(ptask));
        }
        wake_cv_.notify_one(); // достаточно разбудить одного свободного рабочего
        return task_id;
    }

    T request_result(size_t task_id) {
        std::shared_future<T> sfut;
        {
            std::lock_guard<std::mutex> lk(store_mutex_);
            auto it = result_store_.find(task_id);
            if (it == result_store_.end())
                throw std::runtime_error("Неизвестный id задачи: " + std::to_string(task_id));
            sfut = it->second;
        }
        T result = sfut.get(); // ждём без удержания мьютекса
        {
            std::lock_guard<std::mutex> lk(store_mutex_);
            result_store_.erase(task_id);
        }
        return result;
    }

private:
    // Цикл каждого рабочего потока — идентичен Server<T>::process_loop
    void worker_loop(std::stop_token token) {
        for (;;) {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            wake_cv_.wait(lk, [&] {
                return !task_queue_.empty() || token.stop_requested();
            });
            if (task_queue_.empty()) break;

            auto [id, ptask] = std::move(task_queue_.front());
            task_queue_.pop();
            lk.unlock(); // отпускаем мьютекс — другие потоки могут взять следующую задачу

            ptask(); // выполняем → future становится ready
        }
    }

    size_t pool_size_;
    std::vector<std::jthread> pool_; // все рабочие потоки

    std::queue<std::pair<size_t, std::packaged_task<T()>>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable wake_cv_;

    std::unordered_map<size_t, std::shared_future<T>> result_store_;
    std::mutex store_mutex_;

    std::atomic<size_t> id_counter_{0};
    std::atomic<bool>   active_{false};
};
