#ifndef _QUEUE_H
#define _QUEUE_H

#include "types.h"
#include "map.h"

#define BLK_SZ (4096)
#define HASH_SZ (256)
#define DATA_SZ (BLK_SZ - HASH_SZ)

#define DISK_OFFSET(id) (BLK_SZ * (id))

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

int queue_init(struct queue *q);

int queue_evict(struct queue *q, struct block *evict);

struct block *queue_dieout(struct queue *q);

int queue_insert_to_head(struct queue *q, struct block *node, struct block **write_back);

int queue_move_to_head(struct queue *q, struct block *node, struct block **write_back);

struct block *queue_search(struct queue *q, uint32_t blk_id);

#endif
