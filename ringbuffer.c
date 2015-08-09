#include "ringbuffer.h"
#include "mem.h"

struct RingBuffer {
    int size;
    jack_ringbuffer_t *buf;
};

RingBuffer *ringbuf_new(int size)
{
    RingBuffer *rb = malloc_exit(sizeof(RingBuffer));
    rb->size = size;
    rb->buf = jack_ringbuffer_create(sizeof(void *) * size);
    return rb;
}

void ringbuf_free(RingBuffer * rb)
{
    jack_ringbuffer_free(rb->buf);
    free(rb);
}

int ringbuf_size(RingBuffer * rb)
{
    return rb->size;
}

inline int ringbuf_count(RingBuffer * rb)
{
    return jack_ringbuffer_read_space(rb->buf) / sizeof(void *);
}

inline bool ringbuf_put(RingBuffer * rb, void *data)
{
    int writeSpace = jack_ringbuffer_write_space(rb->buf);
    if (writeSpace < sizeof(void *)) {
        return false;
    }
    jack_ringbuffer_write(rb->buf, (char *)(&data), sizeof(void *));
    return true;
}

inline void *ringbuf_get(RingBuffer * rb)
{
    if (ringbuf_count(rb) == 0) {
        return NULL;
    }
    void *data;
    jack_ringbuffer_read(rb->buf, (char *)(&data), sizeof(void *));
    return data;
}
