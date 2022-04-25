#include "types.h"
#include "inode.h"
#include "cache.h"
#include "security.h"
#include "layout.h"
#include "error.h"
#include "disk.h"
#include "block.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern superblock_t sb;

cache_t bcac;

static int bcache_cb_write(void *content, uint32_t id)
{

}

int block_init(void)
{
    return cache_init(&bcac, bcache_cb_write);
}

static block_t *get_block_from_disk(uint32_t bid, uint16_t type, uint16_t iid,
                                    const uint8_t hash_subidx[4], const key128_t *iv,
                                    const key128_t *key, const key256_t *exp_hash)
{
    block_t *bp = (block_t *)malloc(sizeof(block_t));
    if(bp == NULL) return NULL;

    if(0 != disk_read(bp->data, bid)) goto error;

    if(0 != aes128_block_encrypt(iv, key, bp->data)) goto error;

    if(0 != sha256_validate(bp->data, exp_hash)){
        char buf[51] = {0};
        snprintf(buf, 50, "block hash validation failed, bid = %d", bid);
        panic(buf);
        goto error;
    }

    bp->bid = bid;
    bp->type = type;
    if(type == BLK_TP_DATA){
        if(hash_subidx == NULL) goto error;
        memcpy(bp->hash_subidx, hash_subidx, sizeof(hash_subidx[0])*4);
    }
    bp->iid = iid;
    bp->lock = (typeof(bp->lock))PTHREAD_MUTEX_INITIALIZER;

    return bp;

error:
    free(bp);
    return NULL;
}

int block_lock(block_t *bp)
{
    return pthread_mutex_lock(&bp->lock);
}

// must hold block lock
int block_unlock(block_t *bp)
{
    return pthread_mutex_unlock(&bp->lock);
}

block_t *bget_from_cache(uint32_t bid, uint16_t iid,
                                const uint8_t hash_subidx[4], const key128_t *iv,
                                const key128_t *key, const key256_t *exp_hash)
{
    if(cache_is_in(&bcac, bid))
        return cache_try_get(&bcac, bid);

    if(bid < INODE_BITMAP_START){
        panic("cache tries to read superblock");
        return NULL;
    }

    if(bid < INODE_START){
        panic("cache tries to read inode bitmap block");
        return NULL;
    }

    uint16_t type;
    if(bid < BITMAP_START) type = BLK_TP_INODE;
    else if(bid < DATA_START) type = BLK_TP_BITMAP;
    else type = BLK_TP_DATA;

    //get iv and key
    block_t *bp;
    if(type == BLK_TP_INODE || type == BLK_TP_BITMAP){
        bp = get_block_from_disk(bid, type, 0, NULL,
                                    &sb.aes_key[SB_KEY_IDX(bid)],
                                    &sb.aes_iv[SB_KEY_IDX(bid)],
                                    &sb.hash[SB_KEY_IDX(bid)]);
    }else{
        bp = get_block_from_disk(bid, type, iid, hash_subidx, iv, key, exp_hash);
    }

    return cache_insert_get(&bcac, bid, (void *)bp);
}

block_t *bget_from_cache_lock(uint32_t bid, uint16_t iid,
                                        const uint8_t hash_subidx[4], const key128_t *iv,
                                        const key128_t *key, const key256_t *exp_hash)
{
    block_t *bp = bget_from_cache(bid, iid, hash_subidx, iv, key, exp_hash);
    if(bp == NULL) return NULL;

    block_lock(bp);

    return bp;
}

int breturn_to_cache(uint32_t bid)
{
    return cache_return(&bcac, bid);
}

// must hold block lock
int block_unlock_return(block_t *bp)
{
    uint32_t bid = bp->bid;
    return block_unlock(bp) || breturn_to_cache(bid);
}

int block_make_dirty(block_t *bp)
{
    return cache_make_dirty(&bcac, bp->bid);
}

uint32_t block_alloc(void)
{


    // memset to 0

}

int block_free(uint32_t bid)
{

}
