#include <math.h>
#include <pthread.h>
#include <x86intrin.h>
#include "sampler.h"
#include "gui.h"

static void alertErr(const char *msg) {
    GtkWidget *dialog;
    GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
    dialog = gtk_message_dialog_new (GTK_WINDOW(_gui.winMain),
                                     flags,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     msg);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void _gui_update_state()
{
    // fileChooser.
    gtk_widget_set_sensitive(_gui.fileChooser,
                             _gui.state == SAMPLER_STATE_STOPPED);

    // toggleRun.
    gtk_widget_set_sensitive(_gui.toggleRun,
                             _gui.state == SAMPLER_STATE_STOPPED ||
                             _gui.state == SAMPLER_STATE_RUNNING);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_gui.toggleRun),
                                 _gui.state == SAMPLER_STATE_RUNNING);

    // spinWorking.
    if (_gui.state == SAMPLER_STATE_LOADING ||
        _gui.state == SAMPLER_STATE_UNLOADING) {
        gtk_spinner_start(GTK_SPINNER(_gui.spinWorking));
    } else {
        gtk_spinner_stop(GTK_SPINNER(_gui.spinWorking));
    }

    // Level meters.
    gtk_level_bar_set_value(_gui.levelL, 0);
    gtk_level_bar_set_value(_gui.levelR, 0);
    gtk_level_bar_set_value(_gui.levelLSat, 0);
    gtk_level_bar_set_value(_gui.levelRSat, 0);

    // Controls.
    gtk_widget_set_sensitive(_gui.boxCtrls,
                             _gui.state == SAMPLER_STATE_RUNNING);
}

// ----------------------------------------------------------------------------
// gui_load
// ----------------------------------------------------------------------------

static gboolean _loading_timer_cb(gpointer data)
{
    if (_gui.state == SAMPLER_STATE_LOADING) {
        return G_SOURCE_CONTINUE;
    }

    g_free(_gui.loadPath);
    _gui.loadPath = NULL;
    _gui_update_state();

    if(_gui.errMsg != NULL) {
        alertErr(_gui.errMsg);
    }

    return G_SOURCE_REMOVE;
}

static void *_load_thread()
{
    _gui.errMsg = sampler_load(_gui.loadPath);
    _gui.state = sampler_state();
    return NULL;
}

static void gui_load()
{
    if (_gui.state != SAMPLER_STATE_STOPPED) {
        return;
    }

    _gui.state = SAMPLER_STATE_LOADING;
    _gui_update_state();

    // Load samples in background thread.
    _gui.loadPath =
        gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER
                                            (_gui.fileChooser));

    pthread_t thread;
    int status = pthread_create(&thread, NULL, _load_thread, NULL);
    if (status != 0) {
        printf("Failed to create loading thread.\n");
        exit(1);
    }
    // Start a timer to check on progress.
    g_timeout_add(128, _loading_timer_cb, NULL);
}

// ----------------------------------------------------------------------------
// gui_unload
// ----------------------------------------------------------------------------

static gboolean _unloading_timer_cb(gpointer data)
{
    if (_gui.state == SAMPLER_STATE_UNLOADING) {
        return G_SOURCE_CONTINUE;
    }

    _gui_update_state();

    if(_gui.errMsg != NULL) {
        alertErr(_gui.errMsg);
    }

    return G_SOURCE_REMOVE;
}

static void *_unload_thread()
{
    _gui.errMsg = sampler_unload();
    _gui.state = sampler_state();
    return NULL;
}

static void gui_unload()
{
    if (_gui.state != SAMPLER_STATE_RUNNING) {
        return;
    }

    _gui.state = SAMPLER_STATE_UNLOADING;
    _gui_update_state();

    // Unload in background.
    pthread_t thread;
    int status = pthread_create(&thread, NULL, _unload_thread, NULL);
    if (status != 0) {
        printf("Failed to create unloading thread.\n");
        exit(1);
    }
    // Start a timer to check on progress.
    g_timeout_add(128, _unloading_timer_cb, NULL);

}

// ----------------------------------------------------------------------------
// Callbacks.
// ----------------------------------------------------------------------------

static void toggleRun(void *button, void *data)
{
    int run = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_gui.toggleRun));

    if (_gui.state == SAMPLER_STATE_STOPPED && run) {
        gui_load();
    } else if (_gui.state == SAMPLER_STATE_RUNNING && !run) {
        gui_unload();
    }
}

static void saveCtrls(void *btn, void *data) {
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;

    dialog = gtk_file_chooser_dialog_new ("Save File",
                                          GTK_WINDOW(_gui.winMain),
                                          action,
                                          "_Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "_Save",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    chooser = GTK_FILE_CHOOSER (dialog);

    gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
    gtk_file_chooser_set_current_name (chooser, "controls.conf");
    res = gtk_dialog_run (GTK_DIALOG (dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename (chooser);
        sampler_save_controls(filename);
        g_free(filename);
    }

    gtk_widget_destroy (dialog);
}

static void loadCtrls(void *btn, void *data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new ("Open File",
                                          GTK_WINDOW(_gui.winMain),
                                          action,
                                          "_Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "_Open",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        filename = gtk_file_chooser_get_filename (chooser);
        sampler_load_controls(filename);
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
}


static gboolean _sampler_state_cb(gpointer data)
{
    if (_gui.state != SAMPLER_STATE_RUNNING) {
        return G_SOURCE_CONTINUE;
    }

    for (int i = 0; i < CTRL_COUNT; ++i) {
        if (_gui.adjSlider[i] == NULL) {
            continue;
        }
        if (i == CTRL_PITCH_BEND) {
            gtk_adjustment_set_upper(_gui.adjSlider[i], ctrls_max(i));
            gtk_adjustment_set_lower(_gui.adjSlider[i], -ctrls_max(i));
            gtk_adjustment_set_value(_gui.adjSlider[i], ctrls_value_gui(i));
        } else {
            gtk_adjustment_set_upper(_gui.adjSlider[i], ctrls_max(i));
            gtk_adjustment_set_lower(_gui.adjSlider[i], ctrls_min(i));
            gtk_adjustment_set_value(_gui.adjSlider[i], ctrls_value_gui(i));
            gtk_adjustment_set_value(_gui.adjMidi[i], ctrls_midi(i));
        }
    }

    __m128d LR = sampler_get_peaks();

    LR[0] = log10(LR[0]);
    LR[1] = log10(LR[1]);
    LR = (LR + 3) / 3;

    gtk_level_bar_set_value(_gui.levelL, LR[0]);
    gtk_level_bar_set_value(_gui.levelR, LR[1]);
    gtk_level_bar_set_value(_gui.levelLSat, LR[0]);
    gtk_level_bar_set_value(_gui.levelRSat, LR[1]);

    sprintf(_gui.numPlayingBuf, "%d", sampler_num_playing());
    gtk_label_set_text(GTK_LABEL(_gui.lblNumPlaying), _gui.numPlayingBuf);

    return G_SOURCE_CONTINUE;
}

static void _slider_changed_cb(void *adj, void *data)
{
    ctrls_update_direct((uintptr_t) data, gtk_adjustment_get_value(adj));
}

static void _midi_changed_cb(void *adj, void *data)
{
    ctrls_connect_midi((uintptr_t) data, gtk_adjustment_get_value(adj));
}

static void _max_changed_cb(void *adj, void *data)
{
    ctrls_set_max((uintptr_t) data, gtk_adjustment_get_value(adj));
}

// ----------------------------------------------------------------------------
// gui_init
// ----------------------------------------------------------------------------
void gui_init()
{
    _gui.state = SAMPLER_STATE_STOPPED;
    _gui.loadPath = NULL;
    _gui.builder = NULL;
    _gui.winMain = NULL;
    _gui.fileChooser = NULL;
    _gui.toggleRun = NULL;
    _gui.spinWorking = NULL;
    _gui.boxCtrls = NULL;

    for (int i = 0; i < CTRL_COUNT; ++i) {
        _gui.adjSlider[i] = NULL;
        _gui.adjMax[i] = NULL;
        _gui.adjMidi[i] = NULL;
    }
}

// ----------------------------------------------------------------------------
// gui_run.
// ----------------------------------------------------------------------------

static GtkWidget *_widget(char *name)
{
    return GTK_WIDGET(gtk_builder_get_object(_gui.builder, name));
}

static GtkAdjustment *_adjustment(char *name)
{
    return GTK_ADJUSTMENT(gtk_builder_get_object(_gui.builder, name));
}

// Get control pointers from the builder.
static void _get_controls()
{
    _gui.winMain = _widget("winMain");
    _gui.fileChooser = _widget("fileChooser");
    _gui.toggleRun = _widget("toggleRun");
    _gui.spinWorking = _widget("spinWorking");

    _gui.btnSaveCtrls = _widget("btnSaveCtrls");
    _gui.btnLoadCtrls = _widget("btnLoadCtrls");
    _gui.lblNumPlaying = _widget("lblNumPlaying");

    _gui.levelL = GTK_LEVEL_BAR(_widget("levelL"));
    _gui.levelR = GTK_LEVEL_BAR(_widget("levelR"));
    _gui.levelLSat = GTK_LEVEL_BAR(_widget("levelLSat"));
    _gui.levelRSat = GTK_LEVEL_BAR(_widget("levelRSat"));

    _gui.boxCtrls = _widget("gridCtrls");

    // Sliders.
    _gui.adjSlider[CTRL_AMPLIFY] = _adjustment("adjAmp");
    _gui.adjSlider[CTRL_RMS_LOW] = _adjustment("adjRmsLow");
    _gui.adjSlider[CTRL_RMS_HIGH] = _adjustment("adjRmsHigh");
    _gui.adjSlider[CTRL_TRANSPOSE] = _adjustment("adjTrans");
    _gui.adjSlider[CTRL_PITCH_BEND] = _adjustment("adjBend");
    _gui.adjSlider[CTRL_GAMMA_AMP] = _adjustment("adjGammaAmp");
    _gui.adjSlider[CTRL_GAMMA_LAYER] = _adjustment("adjGammaLayer");
    _gui.adjSlider[CTRL_TAU_KEY_UP] = _adjustment("adjTauKeyUp");
    _gui.adjSlider[CTRL_TAU_FADE_IN] = _adjustment("adjTauFadeIn");
    _gui.adjSlider[CTRL_PAN_LOW] = _adjustment("adjPanL");
    _gui.adjSlider[CTRL_PAN_HIGH] = _adjustment("adjPanH");
    _gui.adjSlider[CTRL_MIX_LAYERS] = _adjustment("adjMixLayers");
    _gui.adjSlider[CTRL_SUSTAIN] = _adjustment("adjSus");

    // Max Ctrls.
    _gui.adjMax[CTRL_AMPLIFY] = _adjustment("adjMaxAmp");
    _gui.adjMax[CTRL_RMS_LOW] = _adjustment("adjMaxRmsLow");
    _gui.adjMax[CTRL_RMS_HIGH] = _adjustment("adjMaxRmsHigh");
    _gui.adjMax[CTRL_TRANSPOSE] = _adjustment("adjMaxTrans");
    _gui.adjMax[CTRL_PITCH_BEND] = _adjustment("adjMaxBend");
    _gui.adjMax[CTRL_GAMMA_AMP] = _adjustment("adjMaxGammaAmp");
    _gui.adjMax[CTRL_GAMMA_LAYER] = _adjustment("adjMaxGammaLayer");
    _gui.adjMax[CTRL_TAU_KEY_UP] = _adjustment("adjMaxTauKeyUp");
    _gui.adjMax[CTRL_TAU_FADE_IN] = _adjustment("adjMaxTauFadeIn");
    _gui.adjMax[CTRL_PAN_LOW] = NULL;
    _gui.adjMax[CTRL_PAN_HIGH] = NULL;
    _gui.adjMax[CTRL_MIX_LAYERS] = NULL;
    _gui.adjMax[CTRL_SUSTAIN] = NULL;

    // Midi Ctrls.
    _gui.adjMidi[CTRL_AMPLIFY] = _adjustment("adjMidiAmp");
    _gui.adjMidi[CTRL_RMS_LOW] = _adjustment("adjMidiRmsLow");
    _gui.adjMidi[CTRL_RMS_HIGH] = _adjustment("adjMidiRmsHigh");
    _gui.adjMidi[CTRL_TRANSPOSE] = _adjustment("adjMidiTrans");
    _gui.adjMidi[CTRL_PITCH_BEND] = NULL;
    _gui.adjMidi[CTRL_GAMMA_AMP] = _adjustment("adjMidiGammaAmp");
    _gui.adjMidi[CTRL_GAMMA_LAYER] = _adjustment("adjMidiGammaLayer");
    _gui.adjMidi[CTRL_TAU_KEY_UP] = _adjustment("adjMidiTauKeyUp");
    _gui.adjMidi[CTRL_TAU_FADE_IN] = _adjustment("adjMidiTauFadeIn");
    _gui.adjMidi[CTRL_PAN_LOW] = _adjustment("adjMidiPanL");
    _gui.adjMidi[CTRL_PAN_HIGH] = _adjustment("adjMidiPanH");
    _gui.adjMidi[CTRL_MIX_LAYERS] = _adjustment("adjMidiMixLayers");
    _gui.adjMidi[CTRL_SUSTAIN] = _adjustment("adjMidiSus");

    for (int i = 0; i < CTRL_COUNT; ++i) {
        if (_gui.adjMidi[i] != NULL) {
            gtk_adjustment_set_lower(_gui.adjMidi[i], -1);
            gtk_adjustment_set_upper(_gui.adjMidi[i], 127);
            gtk_adjustment_set_step_increment(_gui.adjMidi[i], 1);
            gtk_adjustment_set_page_increment(_gui.adjMidi[i], 10);
            gtk_adjustment_set_value(_gui.adjMidi[i], -1);
        }
        // Set initial max spinner values.
        if (_gui.adjMax[i] != NULL) {
            gtk_adjustment_set_value(_gui.adjMax[i], ctrls_max(i));
        }
    }
}

static void _connect_callbacks()
{
    g_signal_connect(_gui.winMain, "destroy", gtk_main_quit, NULL);
    g_signal_connect(_gui.toggleRun, "toggled", G_CALLBACK(toggleRun), NULL);

    g_signal_connect(_gui.btnSaveCtrls,
                     "clicked", G_CALLBACK(saveCtrls), NULL);
    g_signal_connect(_gui.btnLoadCtrls,
                     "clicked", G_CALLBACK(loadCtrls), NULL);

    for (uintptr_t i = 0; i < CTRL_COUNT; ++i) {
        if (_gui.adjSlider[i] == NULL) {
            continue;
        }
        // Slider.
        g_signal_connect(_gui.adjSlider[i],
                         "value-changed",
                         G_CALLBACK(_slider_changed_cb), (void *)i);

        // Midi.
        if (_gui.adjMidi[i] != NULL) {
            g_signal_connect(_gui.adjMidi[i],
                             "value-changed",
                             G_CALLBACK(_midi_changed_cb), (void *)i);
        }
        // Max.
        if (_gui.adjMax[i] != NULL) {
            g_signal_connect(_gui.adjMax[i],
                             "value-changed",
                             G_CALLBACK(_max_changed_cb), (void *)i);
        }
    }

    // Run the sampler state callback function indefinitely.
    g_timeout_add(128, _sampler_state_cb, NULL);
}

void gui_run(int argc, char *argv[])
{
    sampler_init();

    _gui.state = SAMPLER_STATE_STOPPED;

    gtk_init(&argc, &argv);

    _gui.builder =
        gtk_builder_new_from_resource("/org/johnnylee/jlsampler/gui.glade");

    // Get controls from builder.
    _get_controls();

    // Free builder memory.
    g_object_unref(G_OBJECT(_gui.builder));

    // Connect callbacks.
    _connect_callbacks();

    // Set the gui state.
    _gui_update_state();

    // Show the main window.
    gtk_widget_show(_gui.winMain);

    // Start the gtk main loop.
    gtk_main();

    sampler_unload();
}
