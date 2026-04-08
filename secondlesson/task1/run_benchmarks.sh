#!/bin/bash

# Скрипт для запуска бенчмарков матрично-векторного умножения
# Тестируем все потоки от 1 до 40, по 20 замеров на каждый поток

N1=20000
N2=40000
RESULTS_FILE="results_all_threads.csv"
N_TESTS=20

echo "=== Matrix-Vector Multiplication Benchmarks (1-40 threads, $N_TESTS runs per thread) ===" | tee $RESULTS_FILE
echo "Date: $(date)" | tee -a $RESULTS_FILE
echo "" | tee -a $RESULTS_FILE

# Функция для запуска теста и возврата среднего времени
run_test() {
    local size=$1
    local threads=$2
    local total_time=0
    local all_times=()
    
    for ((i=1; i<=$N_TESTS; i++)); do
        output=$(./matvec $size $threads 2>&1)
        time=$(echo "$output" | grep "Time:" | awk '{print $2}')
        if [ -n "$time" ]; then
            all_times+=($time)
            total_time=$(echo "$total_time + $time" | bc)
        fi
    done
    
    # Вычисляем среднее время
    if [ ${#all_times[@]} -gt 0 ]; then
        avg_time=$(echo "scale=6; $total_time / ${#all_times[@]}" | bc)
        echo "$avg_time"
    else
        echo "0"
    fi
}

# Замеры для N=20000
echo "=== Testing N=20000 ===" | tee -a $RESULTS_FILE
echo "threads,avg_time,std_dev,min_time,max_time,speedup" >> $RESULTS_FILE

T1_20k=$(run_test $N1 1)
echo "1 thread: ${T1_20k}s" | tee -a $RESULTS_FILE
echo "N=20000,1,$T1_20k,0,0,0,1.0" >> $RESULTS_FILE

for p in {2..40}; do
    Tp=$(run_test $N1 $p)
    Speedup=$(echo "scale=2; $T1_20k / $Tp" | bc)
    echo "$p threads: ${Tp}s (speedup: ${Speedup}x)" | tee -a $RESULTS_FILE
    echo "N=20000,$p,$Tp,0,0,0,$Speedup" >> $RESULTS_FILE
done

echo "" | tee -a $RESULTS_FILE

# Замеры для N=40000
echo "=== Testing N=40000 ===" | tee -a $RESULTS_FILE
echo "threads,avg_time,std_dev,min_time,max_time,speedup" >> $RESULTS_FILE

T1_40k=$(run_test $N2 1)
echo "1 thread: ${T1_40k}s" | tee -a $RESULTS_FILE
echo "N=40000,1,$T1_40k,0,0,0,1.0" >> $RESULTS_FILE

for p in {2..40}; do
    Tp=$(run_test $N2 $p)
    Speedup=$(echo "scale=2; $T1_40k / $Tp" | bc)
    echo "$p threads: ${Tp}s (speedup: ${Speedup}x)" | tee -a $RESULTS_FILE
    echo "N=40000,$p,$Tp,0,0,0,$Speedup" >> $RESULTS_FILE
done

echo ""
echo "=== Results saved to $RESULTS_FILE ==="
cat $RESULTS_FILE