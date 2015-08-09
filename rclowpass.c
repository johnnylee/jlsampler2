#include <math.h>
#include <x86intrin.h>
#include <stdint.h>
#include "global.h"
#include "mem.h"

static double _freq3db(double freq, int order) {
    return freq / sqrt(pow(2.0, 1.0/((double)order)) - 1.0);
}

static void _rcLowPass1(int16_t *x, __m128d *tmp, int len, double freq) {
    double rc = 1.0 / (freq * 2.0 * 3.14);
    double dt = 1.0 / SAMPLE_RATE;
    double alpha = dt / (rc + dt);

    __m128d LR;
    __m128d max = _mm_setzero_pd();

    tmp[0][0] = (double)x[0];
    tmp[0][1] = (double)x[1];

    for(int i = 1; i < len; ++i) {
        LR[0] = (double)x[2*i];
        LR[1] = (double)x[2*i + 1];
        tmp[i] = tmp[i-1] + (alpha*(LR - tmp[i-1]));

        max = _mm_max_pd(max, tmp[i]);
        max = _mm_max_pd(max, -tmp[i]);
    }


    double dMax = max[0];
    if(max[1] > max[0]) {
        dMax = max[1];
    }

    double factor = INT16_MAX / dMax;

    for(int i = 0; i < len; ++i) {
        LR = tmp[i] * factor;
        x[2*i] = (int16_t)LR[0];
        x[2*i+1] = (int16_t)LR[1];
    }
}

void rcLowPass(int16_t *out, int len, double freq, int order) {
    // Find the 3db frequency for a given order.
    freq = _freq3db(freq, order);

    // Allocate a temporary array for our work.
    __m128d *tmp = malloc_exit(len * sizeof(__m128d));

    // Perform the number of low-pass operations requested by `order`.
    for(int i = 0; i < order; ++i) {
        _rcLowPass1(out, tmp, len, freq);
    }

    // Free our temporary memory.
    free(tmp);
}
