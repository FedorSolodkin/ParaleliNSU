#!/bin/bash

NSTEPS=40000000
RESULTS="integration_results_all_threads.csv"
N_TESTS=20

echo "=== Numerical Integration Benchmarks (1-40 threads, $N_TESTS runs per thread) ==="
echo "Number of steps: $NSTEPS"
echo ""

# Проверяем наличие исполняемого файла
if [ ! -f "./main" ]; then
    echo "ERROR: ./main not found! Please run 'make' first."
    exit 1
fi

# Заголовок CSV
echo "threads,avg_time,std_dev,min_time,max_time,speedup" > $RESULTS

# Функция для запуска теста и возврата статистики
run_test() {
    local steps=$1
    local threads=$2
    local total_time=0
    local all_times=()
    
    for ((i=1; i<=$N_TESTS; i++)); do
        output=$(./main $steps $threads 2>&1 | grep "Time:" | awk '{print $2}')
        if [ -n "$output" ]; then
            all_times+=($output)
            total_time=$(echo "$total_time + $output" | bc)
        fi
    done
    
    # Вычисляем среднее время
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
        
        echo "$avg_time,$std_dev,$min_time,$max_time"
    else
        echo "0,0,0,0"
    fi
}

# Базовый замер (1 поток)
echo "Running 1 thread ($N_TESTS runs)..."
result=$(run_test $NSTEPS 1)
T1=$(echo $result | cut -d',' -f1)
std_dev=$(echo $result | cut -d',' -f2)
min_time=$(echo $result | cut -d',' -f3)
max_time=$(echo $result | cut -d',' -f4)

if [ -z "$T1" ] || [ "$T1" == "0" ]; then
    echo "ERROR: Could not extract time! Check program output."
    ./main $NSTEPS 1
    exit 1
fi

echo "1 thread: ${T1}s (std_dev: ${std_dev}, min: ${min_time}, max: ${max_time})"
echo "1,$T1,$std_dev,$min_time,$max_time,1.0" >> $RESULTS

# Остальные потоки от 2 до 40
for p in {2..40}; do
    echo "Running $p threads ($N_TESTS runs)..."
    result=$(run_test $NSTEPS $p)
    Tp=$(echo $result | cut -d',' -f1)
    std_dev=$(echo $result | cut -d',' -f2)
    min_time=$(echo $result | cut -d',' -f3)
    max_time=$(echo $result | cut -d',' -f4)
    
    if [ -n "$Tp" ] && [ "$Tp" != "0" ]; then
        Speedup=$(echo "scale=2; $T1 / $Tp" | bc)
        echo "$p threads: ${Tp}s (speedup: ${Speedup}x, std_dev: ${std_dev})"
        echo "$p,$Tp,$std_dev,$min_time,$max_time,$Speedup" >> $RESULTS
    else
        echo "ERROR: Failed to get time for $p threads"
    fi
done

echo ""
echo "Results saved to $RESULTS"
echo ""
cat $RESULTS

