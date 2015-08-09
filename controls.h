#ifndef CONTROLS_H_
#define CONTROLS_H_

#define CTRL_SUSTAIN 0

#define CTRL_AMPLIFY 1
#define CTRL_GAMMA_AMP 2

#define CTRL_GAMMA_LAYER 3
#define CTRL_MIX_LAYERS 4

#define CTRL_RMS_LOW 5
#define CTRL_RMS_HIGH 6

#define CTRL_PAN_LOW 7
#define CTRL_PAN_HIGH 8

#define CTRL_TAU_KEY_UP 9
#define CTRL_TAU_FADE_IN 10

#define CTRL_TRANSPOSE 11
#define CTRL_PITCH_BEND 12

#define CTRL_COUNT 13

// Controls: All controls are represented by a double value, and have a minimum
// and maximum associated with them. The output control value is computed as
// min + (max-min)*x, where x runs from 0 to 1.
typedef struct {
    int midi[CTRL_COUNT];       // Midi channel for a given control.

    double min[CTRL_COUNT];     // Min value.
    double max[CTRL_COUNT];     // Max value.
    double value[CTRL_COUNT];   // Current committed control value.
    double _value[CTRL_COUNT];  // Uncommitted control values.

    double velocity[128];       // Committed key velocities.
    double _velocity[128];      // Uncommitted key velocities.

    double pitchBendPrev;       // Keep for smoothing.
} Controls;

// There is only one, global controls object.
Controls _ctrls;

// Load default values for all of the controls.
void ctrls_load_defaults();

// Update the control directly, without applying min/max. The range of the
// value will be unchecked.
void ctrls_update_direct(int id, double value);

// Update a control from an input value ranging from 0-1.
void ctrls_update(int id, double value);

// Update a key velocity.
void ctrls_key_update(int key, double velocity);

// Processes a midi control message.
void ctrls_midi_update(int control, double value);

// Commit values into the value array. This should only be called from one
// thread. In our case, we'll only call it from the jack callback thread.
void ctrls_commit();

double ctrls_value_gui(int id);

// Get a control value by it's id.
int ctrls_midi(int id);
double ctrls_value(int id);
double ctrls_min(int id);
double ctrls_max(int id);

void ctrls_set_max(int id, double value);

// Get a key's current velocity. 0 means the key isn't pressed.
double ctrls_key_velocity(int key);

// Get the pitch-bend value from the previous commit.
double ctrls_pitch_bend_prev();

// Return the amplification multiplier for the given key, where vel is the key
// velocity and rms is the sample's measured RMS value.
double ctrls_sample_amp(int key, double vel, double rms);

double ctrls_sample_pan(int key);       // TODO

void ctrls_connect_midi(int control, int midiChan);

#endif                          // CONTROLS_H_
