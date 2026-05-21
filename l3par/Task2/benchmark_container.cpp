// Benchmark: std::map vs std::unordered_map для хранения результатов.
// Операции: insert / find / erase — именно то, что делает сервер с results_.

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr int N = 200'000;

template<typename Map>
void bench(const std::string& name) {
    Map m;

    // ── insert ──────────────────────────────────────────────────────────────
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        m.emplace(static_cast<size_t>(i), 1.0);
    auto t1 = std::chrono::high_resolution_clock::now();

    // ── find ─────────────────────────────────────────────────────────────────
    volatile double sink = 0;
    for (int i = 0; i < N; ++i) {
        auto it = m.find(static_cast<size_t>(i));
        sink += it->second;
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    // ── erase ────────────────────────────────────────────────────────────────
    for (int i = 0; i < N; ++i)
        m.erase(static_cast<size_t>(i));
    auto t3 = std::chrono::high_resolution_clock::now();

    (void)sink;

    auto us = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };

    long ins = us(t0, t1), fnd = us(t1, t2), ers = us(t2, t3);
    std::cout << name << "  (N=" << N << "):\n"
              << "  insert : " << ins << " us\n"
              << "  find   : " << fnd << " us\n"
              << "  erase  : " << ers << " us\n"
              << "  total  : " << ins + fnd + ers << " us\n\n";
}

int main() {
    std::cout << "=== Container Benchmark ===\n\n";

    // Разогрев
    bench<std::unordered_map<size_t, double>>("");

    bench<std::map<size_t, double>>("std::map<size_t,double>");
    bench<std::unordered_map<size_t, double>>("std::unordered_map<size_t,double>");

    std::cout << "Вывод: unordered_map имеет O(1) среднее время против O(log N) у map.\n"
              << "Для контейнера результатов сервера выбираем unordered_map.\n";
    return 0;
}
