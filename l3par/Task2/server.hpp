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

// Однопоточный сервер задач.
//
// Как это работает:
//   1. Клиент вызывает add_task(func) — задача оборачивается в packaged_task,
//      которая автоматически установит shared_future в ready после выполнения.
//   2. shared_future (а не future) выбрана потому что get() можно вызывать
//      несколько раз из разных потоков — обычная future позволяет только один раз.
//   3. Рабочий поток (executor_) спит на condition_variable, пока очередь пуста.
//   4. Результаты хранятся в unordered_map — O(1) avg для insert/find/erase,
//      в отличие от std::map с O(log n).
//
// Шаблонный параметр T — тип возвращаемого значения задач.
template<typename T>
class Server {
public:
    Server() = default;

    // При уничтожении объекта автоматически останавливаем сервер
    ~Server() {
        if (active_.load()) stop();
    }

    // Запуск рабочего потока
    void start() {
        active_ = true;
        // jthread автоматически вызывает request_stop() + join() в деструкторе
        executor_ = std::jthread([this](std::stop_token token) { process_loop(token); });
    }

    // Остановка: сигналим потоку и будим его, чтобы он увидел stop_token
    void stop() {
        if (!active_.exchange(false)) return; // защита от двойного вызова
        executor_.request_stop();
        wake_cv_.notify_all();
    }

    // Добавить задачу в очередь. Возвращает уникальный id для получения результата.
    size_t add_task(std::function<T()> func) {
        std::packaged_task<T()> ptask(std::move(func));
        // share() даёт shared_future — многократный get() из разных потоков
        std::shared_future<T> sfut = ptask.get_future().share();
        const size_t task_id = id_counter_++;

        {
            // Сначала регистрируем future, потом добавляем в очередь.
            // Порядок важен: если сначала добавить в очередь, сервер может
            // выполнить задачу до того, как мы вставим future в result_store_.
            std::lock_guard<std::mutex> lk(store_mutex_);
            result_store_.emplace(task_id, sfut);
        }
        {
            std::lock_guard<std::mutex> lk(queue_mutex_);
            task_queue_.emplace(task_id, std::move(ptask));
        }
        wake_cv_.notify_one(); // будим рабочий поток
        return task_id;
    }

    // Блокирующий запрос результата. Клиент ждёт, пока сервер не выполнит задачу.
    // После получения результата удаляем запись из хранилища (освобождаем память).
    T request_result(size_t task_id) {
        std::shared_future<T> sfut;
        {
            std::lock_guard<std::mutex> lk(store_mutex_);
            auto it = result_store_.find(task_id);
            if (it == result_store_.end())
                throw std::runtime_error("Неизвестный id задачи: " + std::to_string(task_id));
            sfut = it->second;
        }
        // get() вызываем БЕЗ удержания мьютекса — иначе сервер не сможет
        // записать результат (он тоже захватывает store_mutex_ при необходимости).
        T result = sfut.get(); // блокирует до готовности
        {
            std::lock_guard<std::mutex> lk(store_mutex_);
            result_store_.erase(task_id); // чистим память после получения
        }
        return result;
    }

private:
    // Цикл рабочего потока: берём задачи из очереди и выполняем по одной
    void process_loop(std::stop_token token) {
        for (;;) {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            // Спим, пока очередь пуста И не запрошена остановка
            wake_cv_.wait(lk, [&] {
                return !task_queue_.empty() || token.stop_requested();
            });
            // Выходим только когда очередь опустела (обрабатываем остаток)
            if (task_queue_.empty()) break;

            auto [id, ptask] = std::move(task_queue_.front());
            task_queue_.pop();
            lk.unlock(); // отпускаем мьютекс перед выполнением — не блокируем add_task

            ptask(); // выполняем задачу → shared_future переходит в состояние ready
        }
    }

    std::jthread executor_; // единственный рабочий поток

    // Очередь задач: пара (id, packaged_task)
    std::queue<std::pair<size_t, std::packaged_task<T()>>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable wake_cv_; // будит executor_ при добавлении задач

    // Хранилище результатов: id → shared_future
    std::unordered_map<size_t, std::shared_future<T>> result_store_;
    std::mutex store_mutex_;

    std::atomic<size_t> id_counter_{0}; // монотонно растущий счётчик id
    std::atomic<bool>   active_{false};  // флаг: сервер запущен
};
