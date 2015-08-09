#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include "global.h"
#include "sampler.h"
#include "confconfig.h"
#include "conftuning.h"
#include "confcontrols.h"
#include "playingsample.h"
#include "mem.h"

void sampler_init()
{
    errBadState = "The sampler is in the incorrect state.";
    errBadDir = "The directory appears to be invalid.";

    if (pthread_mutex_init(&_sampler.mutex, NULL) != 0) {
        printf("Failed to initialize mutex.\n");
        exit(1);
    }

    _sampler.state = SAMPLER_STATE_STOPPED;

    _sampler.peak = _mm_setzero_pd();

    // Intialize controls and sample storage.
    ctrls_load_defaults();
    sstore_init();

    // Initialize config files.
    confconfig_init();
    conftuning_init();
    confctrls_init();

    // Initialize ring buffers.
    // psNew is twice as big because we always put two playing samples in at a
    // time.
    _sampler.psPlaying = ringbuf_new(RING_BUF_SIZE);
    _sampler.psNew = ringbuf_new(2 * RING_BUF_SIZE);
    _sampler.psRecycle = ringbuf_new(RING_BUF_SIZE);

    // Fill the recycle ring buffer.
    int i;
    for (i = 0; i < RING_BUF_SIZE; ++i) {
        PlayingSample *ps = malloc_exit(sizeof(PlayingSample));
        ringbuf_put(_sampler.psRecycle, ps);
    }

    // Start the midi-reading thread.
    pthread_t thread;
    int status = pthread_create(&thread, NULL, sampler_midi_thread, NULL);
    if (status != 0) {
        printf("Sampler: Failed to create midi processing thread.");
        exit(1);
    }

    // Initialize jack.
    _sampler.jackClient = jack_client_open("JLSampler", JackNullOption, NULL);

    // Create jack output ports.
    _sampler.jackPortL = jack_port_register(_sampler.jackClient, "Out_1",
                                            JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsOutput, 0);

    _sampler.jackPortR = jack_port_register(_sampler.jackClient, "Out_2",
                                            JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsOutput, 0);

    // Set the jack process callback.
    jack_set_process_callback(_sampler.jackClient, sampler_jack_process, NULL);
}

int sampler_state()
{
    return _sampler.state;
}

inline __m128d sampler_get_peaks()
{
    __m128d peak = _sampler.peak;
    _sampler.peak = _mm_setzero_pd();
    return peak;
}

inline int sampler_num_playing()
{
    return ringbuf_count(_sampler.psPlaying);
}

static const char *_sampler_load(char *dir)
{
    if (_sampler.state != SAMPLER_STATE_STOPPED) {
        return errBadState;
    }

    _sampler.state = SAMPLER_STATE_LOADING;

    printf("Sampler: State = Loading\n");
    printf("Directory: %s\n", dir);

    // Attempt to change into the given directory.
    if (chdir(dir) != 0) {
        printf("Failed to change into directory: %s\n", dir);
        _sampler.state = SAMPLER_STATE_STOPPED;
        return errBadDir;
    }
    // Load control defaults.
    ctrls_load_defaults();

    // Load config files.
    confconfig_load();
    conftuning_load();
    confctrls_load("controls.conf");

    // Change into sample directory to load samples.
    if (chdir("./samples") != 0) {
        printf("Failed to change into samples directory.\n");
        _sampler.state = SAMPLER_STATE_STOPPED;
        return errBadDir;
    }
    // Load samples using file info.
    printf("Loading samples...\n");
    sstore_load();

    // Change back to sampler directory.
    if (chdir("../") != 0) {
        printf("Warning: Failed to change out of sample directory.");
    }

    // Fake RC layer.
    if(confconfig_fake_rc_layer() != 0) {
        printf("Creating fake RC layer, order %i...\n",
               confconfig_fake_rc_layer());
        sstore_fake_rc_layer(confconfig_fake_rc_layer());
    }

    // Borrow samples.
    printf("Borrowing samples +/- %i...\n", confconfig_rr_borrow());
    sstore_borrow_samples(confconfig_rr_borrow());

    // Fill samples.
    printf("Filling samples...\n");
    sstore_fill_samples();

    // Crop samples.
    printf("Cropping samples, th = %f...\n", confconfig_crop_thresh());
    sstore_crop(confconfig_crop_thresh());

    // Compute sample RMS values.
    printf("Computing RMS values, dt = %f...\n", confconfig_rms_time());
    sstore_compute_rms(confconfig_rms_time());

    // Unload config files.
    confconfig_unload();
    conftuning_unload();
    confctrls_unload();

    // Activate our jack client.
    printf("Activating Jack client...\n");
    jack_activate(_sampler.jackClient);

    // Done.
    _sampler.state = SAMPLER_STATE_RUNNING;
    return NULL;
}

const char *sampler_load(char *dir)
{
    pthread_mutex_lock(&_sampler.mutex);
    const char *ret = _sampler_load(dir);
    pthread_mutex_unlock(&_sampler.mutex);
    return ret;
}

static void _unload_buffer(RingBuffer * rb)
{
    PlayingSample *ps;
    int count = ringbuf_count(rb);
    for (int i = 0; i < count; ++i) {
        ps = ringbuf_get(rb);
        if (ps != NULL) {
            ringbuf_put(_sampler.psRecycle, ps);
        }
    }
}

const char *_sampler_unload()
{
    if (_sampler.state != SAMPLER_STATE_RUNNING) {
        return errBadState;
    }

    _sampler.state = SAMPLER_STATE_UNLOADING;

    // Stop jack callback.
    printf("Stopping jack client...\n");
    jack_deactivate(_sampler.jackClient);
    sleep(1);

    // Free sample memory.
    printf("Freeing sample memory...\n");
    sstore_free_data();

    // Clear ring buffers.
    printf("Clearing ring buffers...\n");
    _unload_buffer(_sampler.psPlaying);
    _unload_buffer(_sampler.psNew);

    _sampler.state = SAMPLER_STATE_STOPPED;
    return NULL;
}

const char *sampler_unload()
{
    pthread_mutex_lock(&_sampler.mutex);
    const char *ret = _sampler_unload();
    pthread_mutex_unlock(&_sampler.mutex);
    return ret;

}

static void _load_ps(
    PlayingSample *ps, int key, double vel, Sample *sample, double mix)
{
    ps->key = key;
    ps->sample = sample;
    ps->idx = ps->sample->idx0;
    ps->amp = ctrls_sample_amp(key, vel, ps->sample->rms) * mix;
    ps->pan = ctrls_sample_pan(key);

    if (ctrls_value(CTRL_TAU_FADE_IN) == 1) {
        ps->fadeInAmp = 0;
    } else {
        ps->fadeInAmp = 1;
    }
}

// Helper for sampler_midi_thread: playback sample without layer mixing.
static void _sampler_midi_thread_note(int key, double vel)
{
    // Transpose.
    key += (int)ctrls_value(CTRL_TRANSPOSE);

    // Update the controls.
    ctrls_key_update(key, vel);

    // If no velocity, nothing to do.
    if (vel == 0) {
        return;
    }

    Sample *sample1, *sample2;
    double mix1 = sstore_get_samples(key, vel, &sample1, &sample2);
    // If we don't have samples, return.
    if(sample1 == NULL && sample2 == NULL) {
        return;
    }

    PlayingSample *ps = ringbuf_get(_sampler.psRecycle);
    if(ps == NULL) {
        printf("Buffers full. Key: %i Velocity: %f.\n", key, vel);
        return;
    }

    _load_ps(ps, key, vel, sample1, mix1);
    ringbuf_put(_sampler.psNew, ps);

    if(sample2 == NULL) {
        ringbuf_put(_sampler.psNew, NULL);
        return;
    }

    ps = ringbuf_get(_sampler.psRecycle);
    if(ps == NULL) {
        printf("Buffers full. Key: %i Velocity: %f.\n", key, vel);
        return;
    }

    _load_ps(ps, key, vel, sample2, 1 - mix1);
    ringbuf_put(_sampler.psNew, ps);
}

void *sampler_midi_thread()
{
    // We need to open the sequencer before doing anything else.
    snd_seq_t *handle;

    int status = snd_seq_open(&handle, "default", SND_SEQ_OPEN_INPUT, 0);
    if (status != 0) {
        printf("Failed to open sequencer.\n");
        exit(1);
    }
    // Give the client a name.
    status = snd_seq_set_client_name(handle, "JLSampler");
    if (status != 0) {
        printf("Failed to set sequencer client name.\n");
        exit(1);
    }
    // Create a port with write capabilities.
    int caps = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
    int portNum = snd_seq_create_simple_port(handle, "MIDI In", caps,
                                             SND_SEQ_PORT_TYPE_MIDI_GM);

    if (portNum < 0) {
        printf("Failed to create sequencer port.\n");
        exit(1);
    }
    // We need a midi event handle to read incoming midi events.
    snd_seq_event_t *event;

    while (1) {
        status = snd_seq_event_input(handle, &event);
        if (status < 0) {
            printf("Sampler: Failed to read MIDI event. Status: %i\n", status);
            continue;
        }
        // Skip samples if sampler isn't in the running state.
        if (_sampler.state != SAMPLER_STATE_RUNNING) {
            continue;
        }

        switch (event->type) {
        case SND_SEQ_EVENT_NOTEON:
            _sampler_midi_thread_note(event->data.note.note,
                                      (double)(event->data.note.velocity) /
                                      127.0);
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            _sampler_midi_thread_note(event->data.note.note, 0);
            break;
        case SND_SEQ_EVENT_CONTROLLER:
            ctrls_midi_update(event->data.control.param,
                              (double)(event->data.control.value) / 127.0);
            break;
        case SND_SEQ_EVENT_PITCHBEND:
            // The pitch-bend value runs from -8192 to 8191.
            ctrls_update(CTRL_PITCH_BEND,
                         (double)(event->data.control.value) / 8192.0);
            break;
        }
    }
}

// Return 1 if done, 0 to continue playing.
static inline int _proc_ps(PlayingSample * ps, int nframes,
                           double pb, double pbSlope)
{
    __m128d *out = _sampler.jackBuf;
    __m128d sLR;

    Sample *sample = ps->sample;

    double velocity = ctrls_key_velocity(ps->key);
    double tauKeyUp = ctrls_value(CTRL_TAU_KEY_UP);
    double tauFadeIn = ctrls_value(CTRL_TAU_FADE_IN);
    double ctrlAmp = ctrls_value(CTRL_AMPLIFY);

    if (ctrls_value(CTRL_SUSTAIN) > 0.5 || velocity != 0) {
        tauKeyUp = 1;
    }

    for (int i = 0; i < nframes; ++i) {
        // Fade out.
        ps->amp *= tauKeyUp;

        // Fade in.
        ps->fadeInAmp *= tauFadeIn;

        // Get the interpolated value.
        sample_interp(sample, ps->idx, &sLR);

        // Amplify.
        sLR *= ps->amp * (1 - ps->fadeInAmp) * ctrlAmp;

        // Write output.
        // TODO: Panning.
        out[i] += sLR;

        // Update position and pitch-bend.
        ps->idx += pb * sample->speed;
        pb += pbSlope;

        // Done?
        if (ps->idx >= sample->len) {
            return 1;
        }
    }

    return (ps->amp < MIN_AMP);
}

int sampler_jack_process(jack_nframes_t nframes, void *data)
{
    // Commit control values.
    ctrls_commit();

    // Zero internal buffer.
    memset(&(_sampler.jackBuf), 0, nframes * sizeof(__m128d));

    // Add new playing samples to psPlaying.
    // We always read two samples at a time to handle mixing between layers.
    PlayingSample *ps;

    int count = ringbuf_count(_sampler.psNew);
    count -= count % 2;
    while (count--) {
        ps = ringbuf_get(_sampler.psNew);
        if (ps != NULL) {
            ringbuf_put(_sampler.psPlaying, ps);
        }
    }

    // Pre-compute pitch-bend data.
    double pb0 = ctrls_pitch_bend_prev();
    double pb1 = ctrls_value(CTRL_PITCH_BEND);
    double pbSlope = (pb1 - pb0) / (double)nframes;

    // Loop through each playing sample and send to output.
    count = ringbuf_count(_sampler.psPlaying);
    while (count--) {
        ps = ringbuf_get(_sampler.psPlaying);
        if (_proc_ps(ps, nframes, pb0, pbSlope)) {
            ringbuf_put(_sampler.psRecycle, ps);
        } else {
            ringbuf_put(_sampler.psPlaying, ps);
        }
    }

    // Get port arrays.
    float *outL = jack_port_get_buffer(_sampler.jackPortL, nframes);
    float *outR = jack_port_get_buffer(_sampler.jackPortR, nframes);

    __m128d vval;

    // Copy data to output buffers, and scale to range 0-1.
    for (int i = 0; i < nframes; ++i) {
        outL[i] = (float)(_sampler.jackBuf[i][0]) * INT16_SCALE;
        outR[i] = (float)(_sampler.jackBuf[i][1]) * INT16_SCALE;

        vval[0] = outL[i];
        vval[1] = outR[i];
        _sampler.peak = _mm_max_pd(_sampler.peak, vval);
    }

    return 0;
}

void sampler_load_controls(char *path) {
    confctrls_load(path);
    confctrls_unload();
}

const char *sampler_save_controls(char *path) {
    const char *ret = confctrls_save(path);
    confctrls_unload();
    return ret;
}
