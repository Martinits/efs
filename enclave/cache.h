#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"
#include "queue.h"
#include "security.h"

// #define CONTENT_READ  (0)
// #define CONTENT_WRITE (1)

#define CACHE_GET_RO (0)
#define CACHE_GET_RW (1)

// typedef int (*content_cb_read_t)(void **, uint32_t, struct key128 *, struct key256 *);
typedef int (*content_cb_write_t)(void *, uint32_t);

struct cache {
    struct queue fifo, lru;
    content_cb_write_t content_cb_write;
};

int cache_init(struct cache *cac, content_cb_write_t content_cb_write);

int cache_is_in(struct cache *cac, uint32_t id);

int cache_insert(struct cache *cac, uint32_t id, void *content);

void *cache_try_get(struct cache *cac, uint32_t id);

int cache_make_dirty(struct cache *cac, uint32_t id);

int cache_return(struct cache *cac, uint32_t id);

#endif
