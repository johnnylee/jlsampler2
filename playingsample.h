#ifndef PLAYINGSAMPLE_H_
#define PLAYINGSAMPLE_H_

#include "sample.h"

// PlayingSample: Represents a single sample that is currently being playing.
typedef struct {
    int key;                    // The key (midi-note) being played.
    Sample *sample;             // The sample being played.
    double idx;                 // The current playback position.
    double amp;                 // The current amplification.
    double pan;                 // The current pan: -1=left, 1=right.
    double fadeInAmp;           // Fade in amplitude. Starts at 1, fades to 0.

} PlayingSample;

#endif                          // PLAYINGSAMPLE_H_
