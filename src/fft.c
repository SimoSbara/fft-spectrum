#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "fft.h"

void fft(double complex* a, int n, bool invert) 
{
    double PI = acos(-1);
    if (n <= 1)
        return;

    double complex* a0 = malloc(n / 2 * sizeof(double complex));
    double complex* a1 = malloc(n / 2 * sizeof(double complex));
    
    for (int i = 0; 2 * i < n; i++) 
    {
        a0[i] = a[2*i];
        a1[i] = a[2*i+1];
    }
    fft(a0, n / 2, invert);
    fft(a1, n / 2, invert);

    double ang = 2 * PI / n * (invert ? -1 : 1);
    double complex w = 1, wn = cos(ang) + I * sin(ang);

    for (int i = 0; 2 * i < n; i++) 
    {
        a[i] = a0[i] + w * a1[i];
        a[i + n/2] = a0[i] - w * a1[i];

        if (invert) 
        {
            a[i] /= 2;
            a[i + n/2] /= 2;
        }

        w *= wn;
    }

    free(a0);
    free(a1);
}