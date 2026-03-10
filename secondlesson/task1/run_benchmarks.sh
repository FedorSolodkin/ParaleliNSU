#!/bin/bash

# Скрипт для запуска бенчмарков матрично-векторного умножения

N1=20000
N2=40000
THREADS=(1 2 4 7 8 16 20 40)
RESULTS_FILE="results_$(date +%Y%m%d_%H%M%S).csv"

echo "=== Matrix-Vector Multiplication Benchmarks ===" | tee $RESULTS_FILE
echo "Date: $(date)" | tee -a $RESULTS_FILE
echo "" | tee -a $RESULTS_FILE

# Функция для запуска теста
run_test() {
    local size=$1
    local threads=$2
    
    output=$(./matvec $size $threads 2>&1)
    time=$(echo "$output" | grep "Time:" | awk '{print $2}')
    echo "$time"
}

# Замеры для N=20000
echo "=== Testing N=20000 ===" | tee -a $RESULTS_FILE
echo "threads,time,speedup" >> $RESULTS_FILE

T1_20k=$(run_test $N1 1)
echo "1 thread: ${T1_20k}s" | tee -a $RESULTS_FILE
echo "1,$T1_20k,1.0" >> $RESULTS_FILE

for p in "${THREADS[@]}"; do
    [ "$p" -eq 1 ] && continue
    
    Tp=$(run_test $N1 $p)
    Speedup=$(echo "scale=2; $T1_20k / $Tp" | bc)
    echo "$p threads: ${Tp}s (speedup: ${Speedup}x)" | tee -a $RESULTS_FILE
    echo "$p,$Tp,$Speedup" >> $RESULTS_FILE
done

echo "" | tee -a $RESULTS_FILE

# Замеры для N=40000
echo "=== Testing N=40000 ===" | tee -a $RESULTS_FILE
echo "threads,time,speedup" >> $RESULTS_FILE

T1_40k=$(run_test $N2 1)
echo "1 thread: ${T1_40k}s" | tee -a $RESULTS_FILE
echo "1,$T1_40k,1.0" >> $RESULTS_FILE

for p in "${THREADS[@]}"; do
    [ "$p" -eq 1 ] && continue
    
    Tp=$(run_test $N2 $p)
    Speedup=$(echo "scale=2; $T1_40k / $Tp" | bc)
    echo "$p threads: ${Tp}s (speedup: ${Speedup}x)" | tee -a $RESULTS_FILE
    echo "$p,$Tp,$Speedup" >> $RESULTS_FILE
done

echo ""
echo "=== Results saved to $RESULTS_FILE ==="
cat $RESULTS_FILE