#include <cstdio>
#include <omp.h>
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace std::chrono;


void init_parallel(double* A, double* b, int m, int n) {
    #pragma omp parallel
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = m / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);

        for (int i = lb; i <= ub; i++) {
            for (int j = 0; j < n; j++) {
                A[i * n + j] = (i + j) / 10.0;
            }
        }
        
      
        if (threadid == 0) {
            for (int j = 0; j < n; j++) {
                b[j] = j / 10.0;
            }
        }
    }
}


void matrix_vector_product_omp(double* A, double* b, double* c, int m, int n) {
    #pragma omp parallel
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = m / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);

        for (int i = lb; i <= ub; i++) {
            c[i] = 0.0;
            for (int j = 0; j < n; j++) {
                c[i] += A[i * n + j] * b[j];
            }
        }
    }
}

int main(int argc, char** argv) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <matrix_size> [num_threads]" << endl;
        return 1;
    }
    
    int m = stoi(argv[1]);  
    int n = m;              
    int num_threads = (argc > 2) ? stoi(argv[2]) : 1;
    
    omp_set_num_threads(num_threads);
    
    cout << "Matrix size: " << m << "x" << n << endl;
    cout << "Threads: " << num_threads << endl;
    
    
    vector<double> A(m * n);
    vector<double> b(n);
    vector<double> c(m, 0.0);
    
   
    double* A_ptr = A.data();
    double* b_ptr = b.data();
    double* c_ptr = c.data();
    
  
    init_parallel(A_ptr, b_ptr, m, n);

    auto start = high_resolution_clock::now();
    
    matrix_vector_product_omp(A_ptr, b_ptr, c_ptr, m, n);
    
    auto end = high_resolution_clock::now();
    duration<double> elapsed = end - start;
    
  
    cout << fixed << setprecision(3);
    cout << "Time: " << elapsed.count() << " s" << endl;
    cout << "Result[0]: " << c_ptr[0] << endl;
    
    return 0;
}