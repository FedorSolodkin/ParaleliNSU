#!/bin/bash

# Скрипт для запуска бенчмарков матрично-векторного умножения
# Тестируем все потоки от 1 до 40

N1=20000
N2=40000
RESULTS_FILE="results_all_threads.csv"

echo "=== Matrix-Vector Multiplication Benchmarks (1-40 threads) ===" | tee $RESULTS_FILE
echo "Date: $(date)" | tee -a $RESULTS_FILE
echo "" | tee -a $RESULTS_FILE

# Замеры для N=20000
echo "=== Testing N=20000 ===" | tee -a $RESULTS_FILE
echo "threads,time,speedup" >> $RESULTS_FILE

./matvec $N1 1 > /tmp/t1_20k.txt 2>&1
T1_20k=$(grep "Time:" /tmp/t1_20k.txt | awk '{print $2}')
echo "1 thread: ${T1_20k}s" | tee -a $RESULTS_FILE
echo "N=20000,1,$T1_20k,1.0" >> $RESULTS_FILE

for p in {2..40}; do
    ./matvec $N1 $p > /tmp/tp_20k.txt 2>&1
    Tp=$(grep "Time:" /tmp/tp_20k.txt | awk '{print $2}')
    Speedup=$(echo "scale=2; $T1_20k / $Tp" | bc)
    echo "$p threads: ${Tp}s (speedup: ${Speedup}x)" | tee -a $RESULTS_FILE
    echo "N=20000,$p,$Tp,$Speedup" >> $RESULTS_FILE
done

echo "" | tee -a $RESULTS_FILE

# Замеры для N=40000
echo "=== Testing N=40000 ===" | tee -a $RESULTS_FILE
echo "threads,time,speedup" >> $RESULTS_FILE

./matvec $N2 1 > /tmp/t1_40k.txt 2>&1
T1_40k=$(grep "Time:" /tmp/t1_40k.txt | awk '{print $2}')
echo "1 thread: ${T1_40k}s" | tee -a $RESULTS_FILE
echo "N=40000,1,$T1_40k,1.0" >> $RESULTS_FILE

for p in {2..40}; do
    ./matvec $N2 $p > /tmp/tp_40k.txt 2>&1
    Tp=$(grep "Time:" /tmp/tp_40k.txt | awk '{print $2}')
    Speedup=$(echo "scale=2; $T1_40k / $Tp" | bc)
    echo "$p threads: ${Tp}s (speedup: ${Speedup}x)" | tee -a $RESULTS_FILE
    echo "N=40000,$p,$Tp,$Speedup" >> $RESULTS_FILE
done

echo ""
echo "=== Results saved to $RESULTS_FILE ==="
cat $RESULTS_FILE