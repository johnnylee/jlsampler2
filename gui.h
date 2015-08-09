#ifndef GUI_H_
#define GUI_H_
#include <gtk/gtk.h>
#include "controls.h"

typedef struct {
    int state;
    char *loadPath;
    const char *errMsg; // From the sampler load/unload.

    GtkBuilder *builder;
    GtkWidget *winMain;

    GtkWidget *fileChooser;
    GtkWidget *toggleRun;
    GtkWidget *spinWorking;

    GtkWidget *btnSaveCtrls;
    GtkWidget *btnLoadCtrls;
    GtkWidget *lblNumPlaying;

    char numPlayingBuf[32];     // Larger than necessary.

    GtkLevelBar *levelL, *levelR, *levelLSat, *levelRSat;

    GtkWidget *boxCtrls;

    GtkAdjustment *adjSlider[CTRL_COUNT];
    GtkAdjustment *adjMax[CTRL_COUNT];
    GtkAdjustment *adjMidi[CTRL_COUNT];
} Gui;

// Global Gui object.
Gui _gui;

void gui_init();

void gui_run(int argc, char *argv[]);

#endif                          // GUI_H_
