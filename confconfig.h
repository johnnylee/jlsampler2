#ifndef CONFCONFIG_H_
#define CONFCONFIG_H_

#include <glib.h>

typedef struct {
    GKeyFile *keyFile;
} ConfConfig;

// Global ConfConfig object.
ConfConfig _confConfig;

void confconfig_init();
void confconfig_load();
void confconfig_unload();

int confconfig_rr_borrow();
int confconfig_fake_rc_layer();
double confconfig_crop_thresh();
double confconfig_rms_time();

#endif                          // CONFCONFIG_H_
