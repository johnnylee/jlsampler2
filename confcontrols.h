#ifndef CONFCTRLS_H_
#define CONFCTRLS_H_

#include <glib.h>
#include "controls.h"

const char *errSaveFailed;

typedef struct {
    GKeyFile *keyFile;
    char *name[CTRL_COUNT];
} ConfCtrls;

// Global Confctrls object.
ConfCtrls _confCtrls;

void confctrls_init();
void confctrls_load(char *path);
void confctrls_unload();
const char *confctrls_save(char *path);

#endif                          // CONFCTRLS_H_
