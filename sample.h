#ifndef SAMPLE_H_
#define SAMPLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <x86intrin.h>
#include "global.h"

typedef struct {
    bool owner;                 // true if sample owns data.
    int len;                    // The number of samples in each channel.
    int idx0;                   // The first sample to play.
    double rms;                 // The RMS value of the initial samples.
    double speed;               // The playback speed multiplier.
    int16_t *data;              // Left/right interleaved data.
} Sample;

void sample_interp(Sample * sample, double idx, __m128d * LR);

typedef struct {
    int numLayers[128];

    int numSamples[128][MAX_LAYERS];
    int rrIdx[128][MAX_LAYERS];

    Sample sample[128][MAX_LAYERS][MAX_VARS];
} SampleStore;

// There is only one, global SampleStore.
SampleStore _sStore;

void sstore_init();

void sstore_load();

void sstore_free_data();

void sstore_crop(double th);

void sstore_compute_rms(double dt);

void sstore_fill_samples();

void sstore_borrow_samples(int maxNotes);

void sstore_fake_rc_layer(int order);

// Return sample 1 mix amplification.
double sstore_get_samples(int key, double vel, Sample **s1, Sample **s2);

#endif                          // SAMPLE_H_
