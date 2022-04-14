#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"
#include "queue.h"

#define CONTENT_READ  (0)
#define CONTENT_WRITE (1)

#define CACHE_GET_RO (0)
#define CACHE_GET_RW (1)

typedef int (*content_free_cb_t)(void *, uint32_t, int);

struct cache {
    struct queue fifo, lru;
    content_free_cb_t content_cb;
};

int cache_init(struct cache *cac, content_free_cb_t content_cb);

void *cache_get(struct cache *cac, uint32_t id, int rw);

int cache_return(struct cache *cac, uint32_t id, int rw);

#endif
