#include <math.h>
#include "global.h"
#include "mem.h"
#include "controls.h"

void ctrls_load_defaults()
{
    // Zero values.
    for (int i = 0; i < CTRL_COUNT; ++i) {
        _ctrls._value[i] = 0;
    }

    // Minimums.
    _ctrls.min[CTRL_SUSTAIN] = 0;
    _ctrls.min[CTRL_AMPLIFY] = 0;
    _ctrls.min[CTRL_GAMMA_AMP] = 0.01;
    _ctrls.min[CTRL_GAMMA_LAYER] = 0.01;
    _ctrls.min[CTRL_MIX_LAYERS] = 0;
    _ctrls.min[CTRL_RMS_HIGH] = 0;
    _ctrls.min[CTRL_RMS_LOW] = 0;
    _ctrls.min[CTRL_PAN_HIGH] = -1;
    _ctrls.min[CTRL_PAN_LOW] = -1;
    _ctrls.min[CTRL_TAU_KEY_UP] = 0;
    _ctrls.min[CTRL_TAU_FADE_IN] = 0;
    _ctrls.min[CTRL_TRANSPOSE] = -12;
    _ctrls.min[CTRL_PITCH_BEND] = 0;

    // Maximums.
    _ctrls.max[CTRL_SUSTAIN] = 1;
    _ctrls.max[CTRL_AMPLIFY] = 4;
    _ctrls.max[CTRL_GAMMA_AMP] = 3;
    _ctrls.max[CTRL_GAMMA_LAYER] = 3;
    _ctrls.max[CTRL_MIX_LAYERS] = 1;
    _ctrls.max[CTRL_RMS_HIGH] = 0.1;
    _ctrls.max[CTRL_RMS_LOW] = 0.5;
    _ctrls.max[CTRL_PAN_HIGH] = 1;
    _ctrls.max[CTRL_PAN_LOW] = 1;
    _ctrls.max[CTRL_TAU_KEY_UP] = 250;
    _ctrls.max[CTRL_TAU_FADE_IN] = 10;
    _ctrls.max[CTRL_TRANSPOSE] = 12;
    _ctrls.max[CTRL_PITCH_BEND] = 1;

    // Non-zero values.
    ctrls_update_direct(CTRL_SUSTAIN, 0);
    ctrls_update_direct(CTRL_AMPLIFY, 1);
    ctrls_update_direct(CTRL_GAMMA_AMP, 2.2);
    ctrls_update_direct(CTRL_GAMMA_LAYER, 1);
    ctrls_update_direct(CTRL_RMS_LOW, 0.25);
    ctrls_update_direct(CTRL_RMS_HIGH, 0.04);
    ctrls_update_direct(CTRL_TAU_KEY_UP, 150);
    ctrls_update_direct(CTRL_TAU_FADE_IN, 0.15);
    ctrls_update_direct(CTRL_PITCH_BEND, 0);

    // Clear velocity.
    for (int i = 0; i < 128; ++i) {
        _ctrls._velocity[i] = 0;
    }

    // Clear midi mapping.
    for (int i = 0; i < CTRL_COUNT; ++i) {
        _ctrls.midi[i] = -1;
    }

    // Our previous pitch bend value is zero.
    _ctrls.pitchBendPrev = 0;

    ctrls_commit();
}

void ctrls_update_direct(int id, double value)
{
    if (id == CTRL_TAU_KEY_UP || id == CTRL_TAU_FADE_IN) {
        // Controls for time-constants are converted into per-sample amplitudes
        // multiple before being stored.
        if (value <= 0) {
            value = 1;
        } else {
            value = exp(-1000.0 / (SAMPLE_RATE * value));
        }
    } else if (id == CTRL_PITCH_BEND) {
        // We convert the pitch-bend value into a time step multiplier.
        value = pow(2, value / 12);
    }

    _ctrls._value[id] = value;
}

void ctrls_update(int id, double value)
{
    value = _ctrls.min[id] + (_ctrls.max[id] - _ctrls.min[id]) * value;
    ctrls_update_direct(id, value);
}

void ctrls_key_update(int key, double vel)
{
    _ctrls._velocity[key] = vel;
}

void ctrls_midi_update(int control, double value)
{
    for (int i = 0; i < CTRL_COUNT; ++i) {
        if (_ctrls.midi[i] == control) {
            ctrls_update(i, value);
        }
    }
}

void ctrls_commit()
{
    _ctrls.pitchBendPrev = _ctrls.value[CTRL_PITCH_BEND];

    for (int i = 0; i < CTRL_COUNT; ++i) {
        _ctrls.value[i] = _ctrls._value[i];
    }

    for (int i = 0; i < 128; ++i) {
        _ctrls.velocity[i] = _ctrls._velocity[i];
    }
}

inline double ctrls_value_gui(int id)
{
    if (id == CTRL_TAU_KEY_UP || id == CTRL_TAU_FADE_IN) {
        if (_ctrls.value[id] == 0) {
            return 0;
        } else {
            return -1000.0 / (SAMPLE_RATE * log(_ctrls.value[id]));
        }
    } else if (id == CTRL_PITCH_BEND) {
        if (_ctrls.value[id] == 0) {
            return 0;
        } else {
            return 12 * log(_ctrls.value[id]) / log(2);
        }
    }
    return _ctrls.value[id];
}

inline int ctrls_midi(int id) {
    return _ctrls.midi[id];
}

inline double ctrls_value(int id)
{
    return _ctrls.value[id];
}

inline double ctrls_min(int id)
{
    return _ctrls.min[id];
}

inline double ctrls_max(int id)
{
    return _ctrls.max[id];
}

inline void ctrls_set_max(int id, double value)
{
    _ctrls.max[id] = value;
    if (id == CTRL_TRANSPOSE) {
        _ctrls.min[id] = -value;
    }
}

inline double ctrls_key_velocity(int key)
{
    return _ctrls.velocity[key];
}

inline double ctrls_pitch_bend_prev()
{
    return _ctrls.pitchBendPrev;
}

inline double ctrls_sample_amp(int key, double vel, double rms)
{
    if (rms == 0) {
        return 0;
    }

    double rmsLow = ctrls_value(CTRL_RMS_LOW);
    double rmsHigh = ctrls_value(CTRL_RMS_HIGH);
    double gammaAmp = ctrls_value(CTRL_GAMMA_AMP);

    double m = (rmsHigh - rmsLow) / 87;
    double amp = (rmsLow + m * ((double)key - 21)) / rms;

    return amp * pow(vel, gammaAmp);
}

inline double ctrls_sample_pan(int key)
{
    double panHigh = ctrls_value(CTRL_PAN_HIGH);
    double panLow = ctrls_value(CTRL_PAN_LOW);
    double m = (panHigh - panLow) / 87.0;
    return panLow + m * ((double)key - 21);
}

void ctrls_connect_midi(int control, int midi)
{
    _ctrls.midi[control] = midi;
}
