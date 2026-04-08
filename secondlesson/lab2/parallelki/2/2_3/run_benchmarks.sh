#!/bin/bash

# Скрипт для запуска бенчмарков Lab 2 (сравнение двух вариантов)
# Тестируем все потоки от 1 до 40, по 20 замеров на каждый поток

RESULTS_V1="results_v1_all_threads.txt"
RESULTS_V2="results_v2_all_threads.txt"
CSV_V1="results_v1_all_threads.csv"
CSV_V2="results_v2_all_threads.csv"
N_TESTS=20
N=13000

echo "=== Lab 2 Benchmarks (1-40 threads, $N_TESTS runs per thread) ==="
echo "Matrix size: N=$N"
echo ""

# Проверяем наличие исполняемого файла
if [ ! -f "./main" ]; then
    echo "ERROR: ./main not found! Please build the project first."
    exit 1
fi

# Функция для запуска тестов и вычисления среднего времени
run_tests() {
    local variant=$1
    local threads=$2
    local total_time=0
    local times=()
    
    for ((i=1; i<=$N_TESTS; i++)); do
        output=$(./main $variant $threads 2>&1)
        time=$(echo "$output" | grep "time=" | sed 's/.*time=\([^,]*\).*/\1/')
        if [ -n "$time" ]; then
            times+=($time)
            total_time=$(echo "$total_time + $time" | bc)
        fi
    done
    
    # Вычисляем среднее время
    avg_time=$(echo "scale=6; $total_time / ${#times[@]}" | bc)
    echo "$avg_time"
}

# Замеры для варианта 1
echo "=== Testing Variant 1 ===" | tee $RESULTS_V1
echo "threads,avg_time,std_dev,min_time,max_time" > $CSV_V1

for p in {1..40}; do
    echo "Running variant 1 with $p threads ($N_TESTS runs)..."
    
    # Собираем все замеры
    all_times=()
    total_time=0
    
    for ((i=1; i<=$N_TESTS; i++)); do
        output=$(./main 1 $p 2>&1)
        time=$(echo "$output" | grep "time=" | sed 's/.*time=\([^,]*\).*/\1/')
        if [ -n "$time" ]; then
            all_times+=($time)
            total_time=$(echo "$total_time + $time" | bc)
        fi
    done
    
    # Вычисляем статистику
    if [ ${#all_times[@]} -gt 0 ]; then
        avg_time=$(echo "scale=6; $total_time / ${#all_times[@]}" | bc)
        
        # Вычисляем стандартное отклонение
        sum_sq_diff=0
        for t in "${all_times[@]}"; do
            diff=$(echo "$t - $avg_time" | bc)
            sq_diff=$(echo "$diff * $diff" | bc)
            sum_sq_diff=$(echo "$sum_sq_diff + $sq_diff" | bc)
        done
        variance=$(echo "scale=6; $sum_sq_diff / ${#all_times[@]}" | bc)
        std_dev=$(echo "scale=6; sqrt($variance)" | bc)
        
        # Находим мин и макс
        min_time=${all_times[0]}
        max_time=${all_times[0]}
        for t in "${all_times[@]}"; do
            if (( $(echo "$t < $min_time" | bc -l) )); then
                min_time=$t
            fi
            if (( $(echo "$t > $max_time" | bc -l) )); then
                max_time=$t
            fi
        done
        
        echo "Variant 1, $p threads: avg=$avg_time, std_dev=$std_dev, min=$min_time, max=$max_time" | tee -a $RESULTS_V1
        echo "$p,$avg_time,$std_dev,$min_time,$max_time" >> $CSV_V1
    else
        echo "ERROR: Failed to get any valid times for $p threads" | tee -a $RESULTS_V1
    fi
done

echo ""

# Замеры для варианта 2
echo "=== Testing Variant 2 ===" | tee $RESULTS_V2
echo "threads,avg_time,std_dev,min_time,max_time" > $CSV_V2

for p in {1..40}; do
    echo "Running variant 2 with $p threads ($N_TESTS runs)..."
    
    # Собираем все замеры
    all_times=()
    total_time=0
    
    for ((i=1; i<=$N_TESTS; i++)); do
        output=$(./main 2 $p 2>&1)
        time=$(echo "$output" | grep "time=" | sed 's/.*time=\([^,]*\).*/\1/')
        if [ -n "$time" ]; then
            all_times+=($time)
            total_time=$(echo "$total_time + $time" | bc)
        fi
    done
    
    # Вычисляем статистику
    if [ ${#all_times[@]} -gt 0 ]; then
        avg_time=$(echo "scale=6; $total_time / ${#all_times[@]}" | bc)
        
        # Вычисляем стандартное отклонение
        sum_sq_diff=0
        for t in "${all_times[@]}"; do
            diff=$(echo "$t - $avg_time" | bc)
            sq_diff=$(echo "$diff * $diff" | bc)
            sum_sq_diff=$(echo "$sum_sq_diff + $sq_diff" | bc)
        done
        variance=$(echo "scale=6; $sum_sq_diff / ${#all_times[@]}" | bc)
        std_dev=$(echo "scale=6; sqrt($variance)" | bc)
        
        # Находим мин и макс
        min_time=${all_times[0]}
        max_time=${all_times[0]}
        for t in "${all_times[@]}"; do
            if (( $(echo "$t < $min_time" | bc -l) )); then
                min_time=$t
            fi
            if (( $(echo "$t > $max_time" | bc -l) )); then
                max_time=$t
            fi
        done
        
        echo "Variant 2, $p threads: avg=$avg_time, std_dev=$std_dev, min=$min_time, max=$max_time" | tee -a $RESULTS_V2
        echo "$p,$avg_time,$std_dev,$min_time,$max_time" >> $CSV_V2
    else
        echo "ERROR: Failed to get any valid times for $p threads" | tee -a $RESULTS_V2
    fi
done

echo ""
echo "=== Results saved to $RESULTS_V1, $RESULTS_V2, $CSV_V1, $CSV_V2 ==="
