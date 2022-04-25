#ifndef _BLOCK_H
#define _BLOCK_H

#include "types.h"
#include "security.h"
#include "layout.h"
#include <pthread.h>

#define BLK_TP_INODE  (0)
#define BLK_TP_BITMAP (1)
#define BLK_TP_DATA   (2)
// #define BLK_TP_INDEX  (3)
#define HASH_IS_IN_INODE (0)
#define HASH_IS_IN_INDEX (1)
typedef struct {
    uint16_t type;
    uint16_t iid;
    uint8_t hash_subidx[4];
    uint32_t bid;
    pthread_mutex_t lock;
    uint8_t data[BLK_SZ];
} block_t;

int block_init(void);

int block_lock(block_t *bp);

int block_unlock(block_t *bp);

block_t *bget_from_cache(uint32_t bid, uint16_t iid,
                            const uint8_t hash_subidx[4], const key128_t *iv,
                            const key128_t *key, const key256_t *exp_hash);

block_t *bget_from_cache_lock(uint32_t bid, uint16_t iid,
                                const uint8_t hash_subidx[4], const key128_t *iv,
                                const key128_t *key, const key256_t *exp_hash);

int breturn_to_cache(uint32_t bid);

int block_unlock_return(block_t *bp);

int block_make_dirty(block_t *bp);

uint32_t block_alloc(void);

int block_free(uint32_t bid);

#endif
