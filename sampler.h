#ifndef SAMPLER_H_
#define SAMPLER_H_
#include <pthread.h>
#include <x86intrin.h>
#include <jack/jack.h>
#include "controls.h"
#include "global.h"
#include "sample.h"
#include "ringbuffer.h"

// Explicity states for the sampler to be in.
#define SAMPLER_STATE_STOPPED 0
#define SAMPLER_STATE_LOADING 1
#define SAMPLER_STATE_RUNNING 2
#define SAMPLER_STATE_UNLOADING 3

const char *errBadState;
const char *errBadDir;

typedef struct Sampler Sampler;

// Sampler: The sampler.
struct Sampler {
    pthread_mutex_t mutex;      // For public functions, not midi / playback.
    int state;

    // Peak left and right values.
    __m128d peak;

    // We need three lock-free ring-buffers to organize our playing samples.
    RingBuffer *psPlaying;      // Currently playing samples.
    RingBuffer *psNew;          // New samples since last callback.
    RingBuffer *psRecycle;      // Recycled playing samples.

    // Local,jack buffer.
    __m128d jackBuf[JACK_BUF_SIZE];

    // Jack client and ports.
    jack_client_t *jackClient;
    jack_port_t *jackPortL, *jackPortR;
};

// There is only one, global sampler object.
Sampler _sampler;

void sampler_init();

// sampler_midi_thread: A background thread that will continually read
// and process midi events for the sampler.
void *sampler_midi_thread();

// jack callback function.
int sampler_jack_process(jack_nframes_t nframes, void *data);

/*****************************************************************************
 * "Public" interface.
 *****************************************************************************/

int sampler_state();

// Info.
__m128d sampler_get_peaks();
int sampler_num_playing();

// sampler_load: Load the sampler from the directory. Return 0 if successful.
const char *sampler_load(char *dir);

// sampler_unload: Unload samples from memory and stop processing midi and jack
// events.
const char *sampler_unload();

void sampler_load_controls(char *path);
const char *sampler_save_controls(char *path);

#endif                          // SAMPLER_H_
