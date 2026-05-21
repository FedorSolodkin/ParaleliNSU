#!/bin/bash
set -e

echo "========================================"
echo "       СБОРКА И ЗАПУСК ЛАБ C++"
echo "========================================"

# ── ЛАБА 1: умножение матрицы на вектор ──────────────────────────────────────
echo ""
echo ">>> [ЛАБА 1] Сборка..."
cd /mnt/c/paral/l3par/Task1
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 > /dev/null
make -j$(nproc)
echo ">>> [ЛАБА 1] Запуск бенчмарка..."
./matvec
echo ""
echo ">>> [ЛАБА 1] Готово. Результаты в Task1/build/results.csv"

# ── ЛАБА 2: клиент-сервер ────────────────────────────────────────────────────
echo ""
echo "========================================"
echo ">>> [ЛАБА 2] Сборка..."
cd /mnt/c/paral/l3par/Task2
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 > /dev/null
make -j$(nproc)

echo ">>> [ЛАБА 2] Запуск основной программы..."
./main

echo ""
echo ">>> [ЛАБА 2] Проверка результатов (обычный сервер)..."
./test_results

echo ""
echo ">>> [ЛАБА 2] Проверка результатов (thread pool сервер)..."
./test_results pool_

echo ""
echo "========================================"
echo "  Всё готово!"
echo "========================================"
