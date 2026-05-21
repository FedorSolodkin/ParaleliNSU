#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// Допустимые погрешности для сравнения вещественных чисел.
// Используем абсолютный порог для sin/sqrt и смешанный для pow.
static constexpr double TOL_ABS = 1e-8;
static constexpr double TOL_REL = 1e-10; // только для pow

// ── Тест файла с результатами sin ────────────────────────────────────────────
// Читаем строки вида "arg result", пересчитываем sin(arg) и сравниваем.
int verify_sin(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open())
        throw std::runtime_error("Не удалось открыть: " + filepath);

    int passed = 0, failed = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue; // пропускаем заголовки
        double arg, got;
        if (!(std::istringstream(line) >> arg >> got)) continue;

        double expected = std::sin(arg);
        if (std::abs(got - expected) > TOL_ABS) {
            std::cerr << std::fixed << std::setprecision(15)
                      << "  FAIL sin(" << arg << "): ожидалось=" << expected
                      << " получено=" << got << "\n";
            ++failed;
        } else {
            ++passed;
        }
    }
    std::cout << "SIN:  " << passed << " прошло, " << failed << " провалено\n";
    return failed;
}

// ── Тест файла с результатами sqrt ───────────────────────────────────────────
int verify_sqrt(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open())
        throw std::runtime_error("Не удалось открыть: " + filepath);

    int passed = 0, failed = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        double arg, got;
        if (!(std::istringstream(line) >> arg >> got)) continue;

        double expected = std::sqrt(arg);
        if (std::abs(got - expected) > TOL_ABS) {
            std::cerr << std::fixed << std::setprecision(15)
                      << "  FAIL sqrt(" << arg << "): ожидалось=" << expected
                      << " получено=" << got << "\n";
            ++failed;
        } else {
            ++passed;
        }
    }
    std::cout << "SQRT: " << passed << " прошло, " << failed << " провалено\n";
    return failed;
}

// ── Тест файла с результатами pow ────────────────────────────────────────────
// Формат строки: "base exponent result"
// Для pow погрешность задаём как max(TOL_ABS, TOL_REL * |expected|),
// потому что при больших числах абсолютная погрешность тоже растёт.
int verify_pow(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open())
        throw std::runtime_error("Не удалось открыть: " + filepath);

    int passed = 0, failed = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        double base_val, exp_val, got;
        if (!(std::istringstream(line) >> base_val >> exp_val >> got)) continue;

        double expected  = std::pow(base_val, exp_val);
        double tolerance = TOL_REL * std::abs(expected) + TOL_ABS;
        if (std::abs(got - expected) > tolerance) {
            std::cerr << std::fixed << std::setprecision(15)
                      << "  FAIL pow(" << base_val << "," << exp_val
                      << "): ожидалось=" << expected << " получено=" << got << "\n";
            ++failed;
        } else {
            ++passed;
        }
    }
    std::cout << "POW:  " << passed << " прошло, " << failed << " провалено\n";
    return failed;
}

// ── Точка входа: ./test_results [prefix] ─────────────────────────────────────
// Опциональный префикс позволяет проверять файлы как обычного сервера,
// так и thread-pool сервера (prefix="pool_").
int main(int argc, char** argv) {
    std::string prefix = (argc > 1) ? argv[1] : "";
    std::cout << "=== Проверка результатов";
    if (!prefix.empty()) std::cout << " (префикс=\"" << prefix << "\")";
    std::cout << " ===\n";

    int total_failed = 0;
    try {
        total_failed += verify_sin (prefix + "results_sin.txt");
        total_failed += verify_sqrt(prefix + "results_sqrt.txt");
        total_failed += verify_pow (prefix + "results_pow.txt");
    } catch (const std::exception& ex) {
        std::cerr << "Ошибка: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\n";
    if (total_failed == 0)
        std::cout << "Все тесты ПРОЙДЕНЫ.\n";
    else
        std::cout << total_failed << " тест(ов) ПРОВАЛЕНО.\n";

    return (total_failed == 0) ? 0 : 1;
}
