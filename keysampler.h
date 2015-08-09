#ifndef KEYSAMPLER_H_
#define KEYSAMPLER_H_
#include "sample.h"
#include "ringbuffer.h"

typedef struct KeySampler KeySampler;

struct KeySampler {
    int original;
    int key;
    int numLayers;
    SampleLayer layer[128];

    RingBuffer *psPlaying;
    RingBuffer *psNew;
    RingBuffer *psRecycle;
};

void keysampler_init(KeySampler * ks);

void keysampler_free_data(KeySampler * ks);

void keysampler_crop(KeySampler * ks, double th);

void keysampler_compute_rms(KeySampler * ks, double dt);

void keysampler_copy(KeySampler * ks, KeySampler * ksFrom);

void keysampler_borrow(KeySampler * ks, KeySampler * ksFrom);

#endif                          // KEYSAMPLER_H_
