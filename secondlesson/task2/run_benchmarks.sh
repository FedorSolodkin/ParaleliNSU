#!/bin/bash

NSTEPS=40000000
RESULTS="integration_results_all_threads.csv"

echo "=== Numerical Integration Benchmarks (1-40 threads) ==="
echo "Number of steps: $NSTEPS"
echo ""

# Проверяем наличие исполняемого файла
if [ ! -f "./main" ]; then
    echo "ERROR: ./main not found! Please run 'make' first."
    exit 1
fi

# Заголовок CSV
echo "threads,time,speedup" > $RESULTS

# Базовый замер (1 поток)
./main $NSTEPS 1 > /tmp/t1.txt 2>&1
T1=$(grep "Time:" /tmp/t1.txt | awk '{print $2}')

if [ -z "$T1" ]; then
    echo "ERROR: Could not extract time! Check program output."
    ./main $NSTEPS 1
    exit 1
fi

echo "1 thread: ${T1}s"
echo "1,$T1,1.0" >> $RESULTS

# Остальные потоки от 2 до 40
for p in {2..40}; do
    ./main $NSTEPS $p > /tmp/tp.txt 2>&1
    Tp=$(grep "Time:" /tmp/tp.txt | awk '{print $2}')
    
    if [ -n "$Tp" ]; then
        Speedup=$(echo "scale=2; $T1 / $Tp" | bc)
        echo "$p threads: ${Tp}s (speedup: ${Speedup}x)"
        echo "$p,$Tp,$Speedup" >> $RESULTS
    else
        echo "ERROR: Failed to get time for $p threads"
    fi
done

echo ""
echo "Results saved to $RESULTS"
echo ""
cat $RESULTS

