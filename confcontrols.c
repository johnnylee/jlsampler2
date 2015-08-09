#include <stdio.h>
#include "confcontrols.h"
#include "controls.h"

void confctrls_init() {
    errSaveFailed = "Failed to save controls file.";

    _confCtrls.keyFile = NULL;

    _confCtrls.name[CTRL_SUSTAIN] = "Sustain";

    _confCtrls.name[CTRL_AMPLIFY] = "Amplify";
    _confCtrls.name[CTRL_GAMMA_AMP] = "GammaAmp";

    _confCtrls.name[CTRL_GAMMA_LAYER] = "GammaLayer";
    _confCtrls.name[CTRL_MIX_LAYERS] = "MixLayers";

    _confCtrls.name[CTRL_RMS_LOW] = "RMSLow";
    _confCtrls.name[CTRL_RMS_HIGH] = "RMSHigh";

    _confCtrls.name[CTRL_PAN_LOW] = "PanLow";
    _confCtrls.name[CTRL_PAN_HIGH] = "PanHigh";

    _confCtrls.name[CTRL_TAU_KEY_UP] = "TauKeyUp";
    _confCtrls.name[CTRL_TAU_FADE_IN] = "TauFadeIn";

    _confCtrls.name[CTRL_TRANSPOSE] = "Transpose";
    _confCtrls.name[CTRL_PITCH_BEND] = "PitchBend";
}

static void _load(int id) {
    char *name = _confCtrls.name[id];
    GError *err = NULL;
    int midi;
    double max, val;

    midi = g_key_file_get_integer(_confCtrls.keyFile, name, "MIDI", &err);
    if(err == NULL) {
        ctrls_connect_midi(id, midi);
    }

    err = NULL;
    max = g_key_file_get_double(_confCtrls.keyFile, name, "Max", &err);
    if(err == NULL) {
        ctrls_set_max(id, max);
    }

    err = NULL;
    val = g_key_file_get_double(_confCtrls.keyFile, name, "Value", &err);
    if(err == NULL) {
        ctrls_update_direct(id, val);
    }
}

void confctrls_load(char *path) {
    confctrls_unload();
    _confCtrls.keyFile = g_key_file_new();

    int status = g_key_file_load_from_file(_confCtrls.keyFile, path,
                                           G_KEY_FILE_NONE, NULL);
    if(!status) {
        printf("Failed to load file: %s\n", path);
        return;
    }

    for(int id = 0; id < CTRL_COUNT; ++id) {
        _load(id);
    }
}

void confctrls_unload() {
    if(_confCtrls.keyFile != NULL) {
         g_key_file_free(_confCtrls.keyFile);
        _confCtrls.keyFile = NULL;
    }
}

static void _save(int id) {
    char *name = _confCtrls.name[id];
    int midi;
    double max, val;

    midi = ctrls_midi(id);
    max = ctrls_max(id);
    val = ctrls_value_gui(id);

    g_key_file_set_integer(_confCtrls.keyFile, name, "MIDI", midi);
    g_key_file_set_double(_confCtrls.keyFile, name, "Max", max);
    g_key_file_set_double(_confCtrls.keyFile, name, "Value", val);
}

const char * confctrls_save(char *path) {
    if(_confCtrls.keyFile != NULL) {
        g_key_file_free(_confCtrls.keyFile);
        _confCtrls.keyFile = NULL;
    }

    _confCtrls.keyFile = g_key_file_new();

    for(int id = 0; id < CTRL_COUNT; ++id) {
        _save(id);
    }

    GError *err = NULL;
    g_key_file_save_to_file(_confCtrls.keyFile, path, &err);
    if(err != NULL) {
        return errSaveFailed;
    }

    return NULL;
}
