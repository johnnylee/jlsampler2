#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

#include <stdbool.h>
#include <jack/ringbuffer.h>

typedef struct RingBuffer RingBuffer;

// ringbuf_new: Create a new RingBuffer with the given capacity.
RingBuffer *ringbuf_new(int size);

// ringbuf_free: Free the buffer. Note that the items in the buffer must be
// freed by the caller before the buffer is freed.
void ringbuf_free(RingBuffer * rb);

// ringbuf_size: Return the total capacity of the buffer.
int ringbuf_size(RingBuffer * rb);

// ringbuf_count: Return the number of items currently in the buffer.
int ringbuf_count(RingBuffer * rb);

// ringbuf_put: Put the given data into the buffer. The return value is true if
// the put was successful, and false if there wasn't room.
bool ringbuf_put(RingBuffer * rb, void *data);

// ringbuffer_get: Return the next item in the buffer. Returns NULL if there
// was no item available.
void *ringbuf_get(RingBuffer * rb);

#endif                          // RINGBUFFER_H_
