#include <stdio.h>
#include <gtk/gtk.h>

#include "gui.h"

int main(int argc, char *argv[])
{
    gui_init();
    gui_run(argc, argv);
}
