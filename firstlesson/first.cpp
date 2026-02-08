#include <bits/stdc++.h>
#include <cmath>
using namespace std;

#ifdef USE_FLOAT
    using real = float;
#else
    using real = double;
#endif
int main() {
	int n = 10000000;
    real *x = new real[n];
    real y = 0;
    for(int i =0;i<n;i++){
        x[i]= sin(i*2*static_cast<real>(M_PI)/static_cast<real>(n));
        y+=x[i];
    }
    cout<<y;
    delete[] x;
    
}
