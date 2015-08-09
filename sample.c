#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sndfile.h>
#include <x86intrin.h>
#include <stdbool.h>

#include "sample.h"
#include "global.h"
#include "controls.h"
#include "conftuning.h"
#include "rclowpass.h"
#include "mem.h"

// ----------------------------------------------------------------------------
// sample_interp
// ----------------------------------------------------------------------------
inline void sample_interp(Sample * sample, double idx, __m128d * LR)
{
    __m128d a, b;
    int i = (int)idx;
    double mu = idx - (double)i;

    a[0] = (double)sample->data[2 * i];
    a[1] = (double)sample->data[2 * i + 1];

    b[0] = (double)sample->data[2 * i + 2];
    b[1] = (double)sample->data[2 * i + 3];

    *LR = (a + mu * (b - a));
}

// Used for both initialization and freeing data.
static void _sstore_init(int freeMem)
{
    int key, layer, var;
    Sample *sample;

    for (key = 0; key < 128; ++key) {
        _sStore.numLayers[key] = 0;
        for (layer = 0; layer < MAX_LAYERS; ++layer) {
            _sStore.numSamples[key][layer] = 0;
            _sStore.rrIdx[key][layer] = 0;
            for (var = 0; var < MAX_VARS; ++var) {
                sample = &(_sStore.sample[key][layer][var]);
                sample->owner = 0;
                sample->len = 0;
                sample->idx0 = 0;
                sample->rms = 0;
                sample->speed = 1;
                if (freeMem && sample->data != NULL && sample->owner) {
                    free(sample->data);
                }
                sample->data = NULL;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// sstore_init
// ----------------------------------------------------------------------------

void sstore_init()
{
    _sstore_init(0);
}

// ----------------------------------------------------------------------------
// sstore_load
// ----------------------------------------------------------------------------

static int _parse_sample_filename(char *name, int *key, int *layer, int *var)
{
    if (sscanf(name, "on-%d-%d-%d", key, layer, var) != 3) {
        return 1;
    }
    if (*key < 0 || *key > 127 ||
        *layer < 1 || *layer > MAX_LAYERS || *var < 1 || *var > MAX_VARS) {
        return 1;
    }
    *layer -= 1;
    *var -= 1;
    return 0;
}

static void _load_sample(Sample * s, char *fn, double st)
{
    if (s->data != NULL) {
        printf("Attempt to load already loaded sample.\n");
        exit(1);
    }

    SF_INFO fileInfo;
    fileInfo.format = 0;
    fileInfo.channels = 2;

    SNDFILE *sndFile = sf_open(fn, SFM_READ, &fileInfo);

    if (fileInfo.channels != 2) {
        printf("Samples must be stereo files.\n");
        return;
    }

    if (fileInfo.samplerate != 48000) {
        printf("Samples must be 48 kHz.\n");
        return;
    }

    s->owner = 1;
    s->len = fileInfo.frames;
    s->idx0 = 0;
    s->rms = 1.0;
    s->speed = pow(2.0, st / 12.0);

    // We over-allocate our array by one sample for each channel.
    // This makes our interpolation code simpler, as we can rely on having an
    // extra zero sample beyond the final one.
    s->data = malloc_exit(2 * (s->len + 1) * sizeof(int16_t));
    int count = sf_read_short(sndFile, s->data, 2 * fileInfo.frames);
    if (count != 2 * fileInfo.frames) {
        printf("Failed to read all samples for file: %s\n", fn);
        printf("    %i != %i\n", count, 2 * (int)fileInfo.frames);
        exit(1);
    }

    if (sf_close(sndFile) != 0) {
        printf("Failed to close file: %s\n", fn);
        return;
    }
    // Zero out the additional left/right samples.
    s->data[2 * s->len] = 0;
    s->data[2 * s->len + 1] = 0;
}

void sstore_load()
{
    DIR *dir;
    struct dirent *entry;
    int key, layer, var, stop;
    double tuning;

    dir = opendir(".");
    if (dir == NULL) {
        printf("Failed to open samples directory.\n");
        exit(1);
    }

    stop = 0;
#pragma omp parallel private(entry, key, layer, var, tuning)
    while (!stop) {
#pragma omp critical
        {
            entry = readdir(dir);
            if (entry == NULL) {
                stop = 1;
                // This silences a warning about an uninitialized variable.
                tuning = 0;
            } else {
                tuning = conftuning_semitones(entry->d_name);
            }
        }

        if (stop) {
            continue;
        }
        // Skip non-regular files and directories.
        if (entry->d_type != DT_REG) {
            continue;
        }
        // Skip incorrectly named files.
        if (_parse_sample_filename(entry->d_name, &key, &layer, &var) != 0) {
            printf("Failed to parse filename: %s\n", entry->d_name);
            continue;
        }

        if (layer >= _sStore.numLayers[key]) {
            _sStore.numLayers[key] = layer + 1;
        }
        if (var >= _sStore.numSamples[key][layer]) {
            _sStore.numSamples[key][layer] = var + 1;
        }
        // Load sample with tuning information.
        _load_sample(&(_sStore.sample[key][layer][var]), entry->d_name,
                     tuning);
    }

    closedir(dir);
}

// ----------------------------------------------------------------------------
// sstore_free_data
// ----------------------------------------------------------------------------

void sstore_free_data()
{
    _sstore_init(1);
}

// ----------------------------------------------------------------------------
// sstore_crop
// ----------------------------------------------------------------------------

static void _crop_sample(Sample * sample, int th)
{
    int i;

    for (i = 0; i < sample->len; ++i) {
        if (sample->data[2 * i] >= th ||
            sample->data[2 * i] <= -th ||
            sample->data[2 * i + 1] >= th || sample->data[2 * i + 1] <= -th) {
            break;
        }
    }
    sample->idx0 = i;
}

void sstore_crop(double thF)
{
    int th = (int)(thF / INT16_SCALE);
    int key, layer, var;

#pragma omp parallel for private(key, layer, var) schedule(dynamic)
    for (key = 0; key < 128; ++key) {
        for (layer = 0; layer < _sStore.numLayers[key]; ++layer) {
            for (var = 0; var < _sStore.numSamples[key][layer]; ++var) {
                _crop_sample(&(_sStore.sample[key][layer][var]), th);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// sstore_compute_rms
// ----------------------------------------------------------------------------

void _compute_sample_rms(Sample * sample, int di)
{
    int iMax = sample->idx0 + di;
    if (iMax > sample->len) {
        iMax = sample->len;
    }

    iMax *= 2;

    double x;
    double rms = 0;
    double count = 0;

    int i;
    for (i = sample->idx0; i < iMax; ++i) {
        ++count;
        x = (double)(sample->data[i] * INT16_SCALE);
        rms += x * x;
    }

    rms /= count;
    rms = sqrt(rms);
    sample->rms = rms;
}

void sstore_compute_rms(double dt)
{
    int key, layer, var;
    int di = (int)(dt * SAMPLE_RATE);

#pragma omp parallel for private(key, layer, var) schedule(dynamic)
    for (key = 0; key < 128; ++key) {
        for (layer = 0; layer < _sStore.numLayers[key]; ++layer) {
            for (var = 0; var < _sStore.numSamples[key][layer]; ++var) {
                _compute_sample_rms(&(_sStore.sample[key][layer][var]), di);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// sstore_fill_samples
// ----------------------------------------------------------------------------

// Return true if a sample was coppied.
static bool _copy_samples(int toKey, int fromKey, bool owned) {
    if(toKey < 0 || toKey > 127 || fromKey < 0 ||
       fromKey > 127 || toKey == fromKey) {
        return false;
    }

    // If fromKey doesn't have any layers, we can't do anything.
    if(!_sStore.numLayers[fromKey]) {
        return false;
    }

    // toKey should have the same number of layers. If it has 0, we'll do
    // a full copy.
    if(_sStore.numLayers[toKey] == 0) {
        _sStore.numLayers[toKey] = _sStore.numLayers[fromKey];
    }

    // The number of layers must be the same - this is true when doing
    // borrowing.
    if(_sStore.numLayers[toKey] != _sStore.numLayers[fromKey]) {
        return false;
    }

    bool coppied = false;

    for(int layer = 0; layer < _sStore.numLayers[toKey]; ++layer) {
        for(int var = 0; var < _sStore.numSamples[fromKey][layer]; ++var) {
            Sample *fromSample = &(_sStore.sample[fromKey][layer][var]);

            // If we're doing borrowing for round-robbin, we only want owned
            // samples.
            if(owned && !fromSample->owner) {
                continue;
            }

            int toVar = _sStore.numSamples[toKey][layer];
            Sample *toSample = &(_sStore.sample[toKey][layer][toVar]);
            *toSample = *fromSample;
            toSample->owner = false;
            toSample->speed =
                fromSample->speed * pow(2, (double)(toKey - fromKey) / 12);
            _sStore.numSamples[toKey][layer]++;
            coppied = true;
        }
    }

    return coppied;
}

void sstore_fill_samples()
{
    for(int i = 1; i < 127; ++i) {

        // Fill from lower key.
        int fromKey = 127;
        while(--fromKey) {
            int toKey = fromKey + 1;
            if(_sStore.numLayers[toKey] == 0 &&
               _sStore.numLayers[fromKey] != 0) {
                _copy_samples(toKey, fromKey, false);
            }
        }

        // Fill from upper key.
        for(int fromKey = 0; fromKey < 127; ++fromKey) {
            int toKey = fromKey - 1;
            if(_sStore.numLayers[toKey] == 0 &&
               _sStore.numLayers[fromKey] != 0) {
                _copy_samples(toKey, fromKey, false);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// sstore_borrow_samples
// ----------------------------------------------------------------------------

void sstore_borrow_samples(int maxDist)
{
    for(int dist = 1; dist < maxDist + 1; ++dist) {
        for(int toKey = 0; toKey < 128; ++toKey) {
            if(_sStore.numLayers[toKey] == 0) {
                continue;
            }
            _copy_samples(toKey, toKey - dist, true);
            _copy_samples(toKey, toKey + dist, true);
        }
    }
}

// ----------------------------------------------------------------------------
// sstore_get_samples
// ----------------------------------------------------------------------------

void _update_rrIdx(int key, int layer) {
    int rrIdx = _sStore.rrIdx[key][layer] + 1;
    _sStore.rrIdx[key][layer] = rrIdx  % _sStore.numSamples[key][layer];
}

// Returns sample 1 mix amplification.
double sstore_get_samples(int key, double vel, Sample **s1, Sample **s2)
{
    *s1 = *s2 = NULL;

    bool mixLayers = ctrls_value(CTRL_MIX_LAYERS) > 0.5;

    int numLayers = _sStore.numLayers[key];
    if(numLayers == 0) {
        return 0;
    }

    // If we're mixing layers, then the maximum value is decreased by 1.
    if(mixLayers) {
        numLayers -= 1;
    }

    // Scale the velocity to find the appropriate layer.
    vel = pow(vel, ctrls_value(CTRL_GAMMA_LAYER));
    double layer = ((double)numLayers * vel);

    // This is the lower layer.
    int layer0 = (int)layer;
    if(layer0 == _sStore.numLayers[key]) {
        --layer0;
    }
    _update_rrIdx(key, layer0);

    *s1 = &(_sStore.sample[key][layer0][_sStore.rrIdx[key][layer0]]);

    // If not mixing layers, we're done.
    if(!mixLayers) {
        return 1;
    }

    // If we're already at the top-most layer, we're done.
    if(layer0 == numLayers) {
        return 1;
    }

    // If the number of samples doesn't match, we don't mix.
    if(_sStore.numSamples[key][layer0] !=
       _sStore.numSamples[key][layer0 + 1]) {
        return 1;
    }

    *s2 = &(_sStore.sample[key][layer0 + 1][_sStore.rrIdx[key][layer0]]);

    return 1 - (layer - (double)layer0);
}

// ----------------------------------------------------------------------------
// sstore_fake_rc_layer
// ----------------------------------------------------------------------------

static void _filter_sample(Sample *s, int order) {
    int16_t * newData = malloc_exit((2 * s->len + 2) * sizeof(int16_t));

    for(int i = 0; i < 2*s->len + 2; ++i) {
        newData[i] = s->data[i];
    }

    rcLowPass(newData, s->len, 10, order);
    s->data = newData;
    s->owner = true;
}

void sstore_fake_rc_layer(int order)
{
    for(int key = 0; key < 128; ++key) {
        if(_sStore.numLayers[key] != 1 || _sStore.numSamples[key][0] == 0) {
            continue;
        }

        _sStore.numLayers[key] = 2;

        // Copy samples to layer 2.
        _sStore.numSamples[key][1] = _sStore.numSamples[key][0];
        for(int var = 0; var < _sStore.numSamples[key][0]; ++var) {
            Sample *s0 = &(_sStore.sample[key][0][var]);
            Sample *s1 = &(_sStore.sample[key][1][var]);

            *s1 = *s0;
            _filter_sample(s0, order);
        }
    }
}
