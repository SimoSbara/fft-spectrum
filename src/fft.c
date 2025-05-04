#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "fft.h"

//https://cp-algorithms.com/algebra/fft.html#implementation
void fft(double complex* a, int n, bool invert) 
{
    double PI = acos(-1);

    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
        {
            double tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
        }
    }

    for (int len = 2; len <= n; len <<= 1) 
    {
        double ang = 2 * PI / len * (invert ? -1 : 1);
        double complex wlen = cos(ang) + I * sin(ang);

        for (int i = 0; i < n; i += len) {
            double complex w = 1;
            for (int j = 0; j < len / 2; j++) {
                double complex  u = a[i+j], v = a[i+j+len/2] * w;
                a[i+j] = u + v;
                a[i+j+len/2] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert) 
    {
        for(int i = 0; i < n; i++)
            a[i] /= (double)n;
    }
}