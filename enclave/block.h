#ifndef _BLOCK_H
#define _BLOCK_H

#include "types.h"
#include "security.h"
#include "layout.h"
#include "inode.h"
#include <pthread.h>

typedef struct {
    union {
        uint16_t iid;
        uint32_t bid;
    };
    uint8_t idx;
} hashidx_t;

#define BLK_TP_INODE  (0)
#define BLK_TP_BITMAP (1)
#define BLK_TP_DATA   (2)
// #define BLK_TP_INDEX  (3)
#define HASH_IS_IN_INODE (0)
#define HASH_IS_IN_INDEX (1)
typedef struct {
    uint16_t type;
    uint32_t bid;
    hashidx_t hashidx[4];
    key128_t aes_iv, aes_key;
    uint8_t data[BLK_SZ];
} block_t;

struct pd_wb_node {
    hashidx_t hashidx[4];
    key256_t hash;
};

int block_init(void);

int block_lock(uint32_t bid);

int block_unlock(uint32_t bid);

block_t *bget_from_cache_lock(uint32_t bid, const hashidx_t hashidx[4], const key128_t *iv,
                                const key128_t *key, const key256_t *exp_hash);

int breturn_to_cache(uint32_t bid);

int block_unlock_return(block_t *bp);

int block_make_dirty(uint32_t bid);

int pd_wb_lock(void);

int pd_wb_unlock(void);

struct pd_wb_node *pd_wb_find(uint32_t bid);

int pd_wb_insert(uint32_t bid);

int pd_wb_delete(uint32_t bid);

uint32_t block_alloc(void);

int block_free(uint32_t bid, int setzero);

uint8_t *bget_raw(uint32_t bid, const key128_t *iv, const key128_t *key, int *in_cache);

void breturn_raw(uint32_t bid, int in_cache, uint8_t *data);

int block_exit(void);

#endif
