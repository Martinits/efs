#ifndef _BLOCK_H
#define _BLOCK_H

#include "types.h"
#include "map.h"

#define BLK_SZ (4096)
#define HASH_SZ (256)
#define DATA_SZ (BLK_SZ - HASH_SZ)

#define BUF_SZ (100)

struct block {
    uint32_t blk_id;
    uint8_t data[BLK_SZ];
    uint32_t refcnt;
    struct block *prev;
    struct block *next;
};

struct buffer {
    struct block blk_list[BUF_SZ];
    struct block fifo_head, lru_head;
    struct map mp;
};

uint8_t* block_get(uint32_t id);

int block_return(uint32_t id, int dirty);

int block_pin(uint32_t id);

#endif
