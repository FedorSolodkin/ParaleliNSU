#!/bin/bash

NSTEPS=40000000
THREADS=(1 2 4 7 8 16 20 40)
RESULTS="integration_results.csv"

echo "=== Numerical Integration Benchmarks ==="
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
echo "Running 1 thread..."
T1=$(./main $NSTEPS 1 2>&1 | grep "Time:" | awk '{print $2}')

if [ -z "$T1" ]; then
    echo "ERROR: Could not extract time! Check program output."
    ./main $NSTEPS 1
    exit 1
fi

echo "1 thread: ${T1}s"
echo "1,$T1,1.0" >> $RESULTS

# Остальные потоки
for p in "${THREADS[@]}"; do
    [ "$p" -eq 1 ] && continue
    
    echo "Running $p threads..."
    Tp=$(./main $NSTEPS $p 2>&1 | grep "Time:" | awk '{print $2}')
    
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

