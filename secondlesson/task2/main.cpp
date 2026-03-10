#include <iostream>
#include <omp.h>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;


double func(double x) {
    return 4.0 / (1.0 + x * x);
}

double integrate_omp(double (*func)(double), double a, double b, long n) {
    double h = (b - a) / n;
    double sum = 0.0;
    
    #pragma omp parallel
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = n / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (n - 1) : (lb + items_per_thread - 1);
        
        double sumloc = 0.0;
        
        for (int i = lb; i <= ub; i++) {
            double x = a + h * (i + 0.5);
            sumloc += func(x);
        }
        
        #pragma omp atomic
        sum += sumloc;
    }
    
    return sum * h;
}

int main(int argc, char** argv) {
    long nsteps = 40000000;  
    int num_threads = 1;
    
    if (argc > 1) {
        nsteps = stol(argv[1]);
    }
    if (argc > 2) {
        num_threads = stoi(argv[2]);
    }
    
    omp_set_num_threads(num_threads);
    
    cout << "Number of steps: " << nsteps << endl;
    cout << "Number of threads: " << num_threads << endl;
    
    
    auto start = high_resolution_clock::now();
    double result = integrate_omp(func, 0.0, 1.0, nsteps);
    auto end = high_resolution_clock::now();
    
    duration<double> elapsed = end - start;
    
    cout << fixed << setprecision(6);
    cout << "Time: " << elapsed.count() << " s" << endl;
    cout << "Result (Pi): " << setprecision(15) << result << endl;
    
    return 0;
}