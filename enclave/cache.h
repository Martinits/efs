#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"
#include "queue.h"
#include "security.h"
#include <pthread.h>

// #define CONTENT_READ  (0)
// #define CONTENT_WRITE (1)

#define CACHE_GET_RO (0)
#define CACHE_GET_RW (1)

// typedef int (*content_cb_read_t)(void **, uint32_t, struct key128 *, struct key256 *);
typedef int (*content_cb_write_t)(void *, uint32_t, int, int);

typedef struct {
    struct queue fifo, lru;
    content_cb_write_t content_cb_write;
    pthread_mutex_t lock;
} cache_t;

int cache_init(cache_t *cac, content_cb_write_t content_cb_write);

void *cache_try_get(cache_t *cac, uint32_t id, int lock, int access);

void **cache_insert_get(cache_t *cac, uint32_t id, int lock);

int cache_make_dirty(cache_t *cac, uint32_t id);

int cache_make_deleted(cache_t *cac, uint32_t id);

int cache_return(cache_t *cac, uint32_t id);

int cache_node_lock(cache_t *cac, uint32_t id);

int cache_node_unlock(cache_t *cac, uint32_t id);

int cache_unlock_return(cache_t *cac, uint32_t id);

int cache_dirty_unlock_return(cache_t *cac, uint32_t id);

int cache_exit(cache_t *cac);

#endif
