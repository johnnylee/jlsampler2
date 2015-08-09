#include "mem.h"

void *malloc_exit(size_t size)
{
    void *data = malloc(size);
    if (data == NULL) {
        exit(1);
    }
    return data;
}

void *calloc_exit(size_t nmemb, size_t size)
{
    void *data = calloc(nmemb, size);
    if (data == NULL) {
        exit(1);
    }
    return data;
}
