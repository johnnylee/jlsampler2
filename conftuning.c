#include <stdio.h>
#include "conftuning.h"

void conftuning_init()
{
    _confTuning.keyFile = NULL;
}

void conftuning_load()
{
    conftuning_unload();
    _confTuning.keyFile = g_key_file_new();

    int status = g_key_file_load_from_file(_confTuning.keyFile, "tuning.conf",
                                           G_KEY_FILE_NONE, NULL);
    if (!status) {
        printf("Failed to load file: tuning.conf.\n");
        return;
    }
}

void conftuning_unload() {
    if (_confTuning.keyFile != NULL) {
        g_key_file_free(_confTuning.keyFile);
        _confTuning.keyFile = NULL;
    }
}

double conftuning_semitones(char *filename)
{
    if (!_confTuning.keyFile) {
        return 0;
    }

    double val =
        g_key_file_get_double(_confTuning.keyFile, "Tuning", filename, NULL);

    return val;
}
