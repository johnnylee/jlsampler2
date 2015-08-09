#ifndef MEM_H_
#define MEM_H_

#include <stdlib.h>

// These allocation functions will exit the program if the memory allocation
// fails.
void *malloc_exit(size_t size);
void *calloc_exit(size_t nmemb, size_t size);

#endif                          // MEM_H_
