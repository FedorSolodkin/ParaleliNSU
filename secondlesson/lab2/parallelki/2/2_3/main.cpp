#include <iostream>
#include <iomanip>
#include <cmath>
#include <omp.h>
#include <vector>
#include <algorithm>

const int N = 13000;           
const double EPS = 1e-5;     
const double TAU = 0.01;    
const double DIAG_VAL = 2.0;   
const double OFF_DIAG_VAL = 1.0; 

void matvec(const std::vector<double>& x, std::vector<double>& y) {
    #pragma omp parallel for
    for (int i = 0; i < N; ++i) {
        double sum = 0.0;
        for (int j = 0; j < N; ++j) {
            if (i == j) {
                sum += DIAG_VAL * x[j];
            } else {
                sum += OFF_DIAG_VAL * x[j];
            }
        }
        y[i] = sum;
    }
}

int solve_variant_1(std::vector<double>& x) {
    std::vector<double> Ax(N);
    std::vector<double> residual(N);
    int iterations = 0;
    
    double norm = 0.0;
    do {
        matvec(x, Ax);
        
        #pragma omp parallel for
        for (int i = 0; i < N; ++i) {
            residual[i] = (N + 1.0) - Ax[i];
        }
        
        #pragma omp parallel for
        for (int i = 0; i < N; ++i) {
            x[i] += TAU * residual[i];
        }
        
        norm = 0.0;
        #pragma omp parallel for reduction(max:norm)
        for (int i = 0; i < N; ++i) {
            double diff = std::abs(x[i] - 1.0); 
            if (diff > norm) norm = diff;
        }
        
        iterations++;
    } while (norm > EPS);
    
    return iterations;
}

int solve_variant_2(std::vector<double>& x) {
    std::vector<double> Ax(N);
    std::vector<double> residual(N);
    int iterations = 0;
    
    double norm = 0.0;
    do {
        #pragma omp parallel
        {
            #pragma omp for
            for (int i = 0; i < N; ++i) {
                double sum = 0.0;
                for (int j = 0; j < N; ++j) {
                    if (i == j) {
                        sum += DIAG_VAL * x[j];
                    } else {
                        sum += OFF_DIAG_VAL * x[j];
                    }
                }
                Ax[i] = sum;
            }
            
            #pragma omp for
            for (int i = 0; i < N; ++i) {
                residual[i] = (N + 1.0) - Ax[i];
            }
            
            #pragma omp for
            for (int i = 0; i < N; ++i) {
                x[i] += TAU * residual[i];
            }
        }
        
        norm = 0.0;
        for (int i = 0; i < N; ++i) {
            double diff = std::abs(x[i] - 1.0);
            if (diff > norm) norm = diff;
        }
        
        iterations++;
    } while (norm > EPS);
    
    return iterations;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <variant: 1 or 2> <num_threads>\n";
        return 1;
    }
    
    int variant = std::stoi(argv[1]);
    int num_threads = std::stoi(argv[2]);
    omp_set_num_threads(num_threads);
    
    std::vector<double> x(N, 0.0);
    
    double t_start = omp_get_wtime();
    
    int iterations = 0;
    if (variant == 1) {
        iterations = solve_variant_1(x);
    } else if (variant == 2) {
        iterations = solve_variant_2(x);
    } else {
        std::cerr << "Variant must be 1 or 2\n";
        return 1;
    }
    
    double t_elapsed = omp_get_wtime() - t_start;
    
    double max_error = 0.0;
    for (int i = 0; i < N; ++i) {
        double error = std::abs(x[i] - 1.0);
        if (error > max_error) max_error = error;
    }
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "N=" << N
              << ", variant=" << variant
              << ", threads=" << num_threads
              << ", time=" << t_elapsed
              << ", iterations=" << iterations
              << ", max_error=" << max_error << "\n";
    
    return 0;
}