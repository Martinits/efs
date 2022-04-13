#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"
#include "queue.h"

#define BLOCK_READ  (0)
#define BLOCK_WRITE (1)

#define BLOCK_GET_RO (0)
#define BLOCK_GET_RW (1)

typedef int (*blockio_callback_t)(uint8_t*, uint32_t, int);

struct cache {
    struct queue fifo, lru;
    blockio_callback_t block_rw;
};

int cache_init(struct cache *cac, blockio_callback_t block_rw);

uint8_t *cache_get_block(struct cache *cac, uint32_t blk_id, int rw);

int cache_return_block(struct cache *cac, uint32_t blk_id, int rw);

#endif
