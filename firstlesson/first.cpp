#include <bits/stdc++.h>
#include <cmath>
#include <vector>
using namespace std;

#ifdef USE_FLOAT
    using real_t = float;
#else
    using real_t = double;
#endif
int main() {
	int n = 10000000;
    vector <real_t>x(n);
    real_t y = 0;
    for(int i =0;i<n;i++){
        x[i]= sin(i*2*static_cast<real_t>(M_PI)/static_cast<real_t>(n));
        y+=x[i];
    }
    cout<<y<<endl;
    
}
