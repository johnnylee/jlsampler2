#ifndef CONFTUNING_H_
#define CONFTUNING_H_

#include <glib.h>

typedef struct {
    GKeyFile *keyFile;
} ConfTuning;

// Global ConfTuning object.
ConfTuning _confTuning;

void conftuning_init();
void conftuning_load();
void conftuning_unload();

double conftuning_semitones(char *filename);

#endif                          // CONFTUNING_H_
