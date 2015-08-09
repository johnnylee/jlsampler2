#include <stdio.h>
#include "confconfig.h"

void confconfig_init()
{
    _confConfig.keyFile = NULL;
}

void confconfig_load()
{
    confconfig_unload();
    _confConfig.keyFile = g_key_file_new();

    int status = g_key_file_load_from_file(_confConfig.keyFile, "config.conf",
                                           G_KEY_FILE_NONE, NULL);
    if (!status) {
        printf("Failed to load file: config.conf.\n");
    }
}

void confconfig_unload()
{
    if(_confConfig.keyFile != NULL) {
         g_key_file_free(_confConfig.keyFile);
        _confConfig.keyFile = NULL;
    }
}

int confconfig_rr_borrow()
{
    if (!_confConfig.keyFile) {
        return 0;
    }

    int val = g_key_file_get_integer(_confConfig.keyFile, "Config", "RRBorrow",
                                     NULL);
    printf("Config RR borrow: %i\n", val);
    return val;

}

int confconfig_fake_rc_layer()
{
    if (!_confConfig.keyFile) {
        return 0;
    }
    int val =
        g_key_file_get_integer(_confConfig.keyFile, "Config", "FakeRCLayer",
                               NULL);
    printf("Config fake RC layer: %i\n", val);
    return val;
}

double confconfig_crop_thresh()
{
    if (!_confConfig.keyFile) {
        return 0;
    }

    double val =
        g_key_file_get_double(_confConfig.keyFile, "Config", "CropThresh",
                              NULL);
    if (val <= 0) {
        val = 0;
    }
    printf("Config crop threshold: %f\n", val);
    return val;
}

double confconfig_rms_time()
{
    if (!_confConfig.keyFile) {
        return 0.25;
    }

    double val =
        g_key_file_get_double(_confConfig.keyFile, "Config", "RMSTime", NULL);
    if (val <= 0) {
        val = 0.25;
    }
    printf("Config rms time: %f\n", val);
    return val;
}
