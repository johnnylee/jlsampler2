#ifndef GLOBAL_H_
#define GLOBAL_H_

#define JACK_BUF_SIZE 16384
#define RING_BUF_SIZE 2048
#define INT16_SCALE 3.0517578125e-05    // For scaling int16 values.
#define SAMPLE_RATE 48000       // Fixed sample rate.
#define MIN_AMP 1e-5            // Minimum amplification before stopping play.

#define MAX_LAYERS 128
#define MAX_VARS 128

#endif                          // GLOBAL_H_
