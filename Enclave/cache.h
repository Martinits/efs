#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"
#include "map.h"

#define BLK_SZ (4096)
#define HASH_SZ (256)
#define DATA_SZ (BLK_SZ - HASH_SZ)

#define BUF_SZ (100)

#define QUEUE_MAX_LEN (256)

struct block {
    union {
        uint32_t blk_id;
        uint32_t list_len;
    };
    uint8_t data[BLK_SZ];
    int refcnt;
    int dirty;
    struct block *prev;
    struct block *next;
};

struct queue {
    struct block head;
    struct map mp;
};

struct cache {
    struct block blk_list[BUF_SZ];
    struct queue fifo, lru;
};

int cache_init(void);

uint8_t *block_get_ro(uint32_t id);

uint8_t *block_get_rw(uint32_t id);

int block_return_ro(uint32_t id);

int block_return_rw(uint32_t id);

int block_pin(uint32_t id);

#endif
