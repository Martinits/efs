#include "types.h"
#include "cache.h"
#include "security.h"
#include "layout.h"
#include "error.h"
#include "disk.h"
#include "block.h"
#include "bitmap.h"
#include "map.h"
#include "superblock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern superblock_t sb;

cache_t bcac;

struct map pd_wb;
pthread_mutex_t pd_wb_lk = PTHREAD_MUTEX_INITIALIZER;


// not holding cache lock now
// no need to lock block because refcnt == 0 so no other is holding it
static int bcache_cb_write(void *content, uint32_t bid, int dirty, int deleted)
{
    block_t *bp = (block_t *)content;

    if(deleted || !dirty){
        free(bp);
        return 0;
    }

    //dirty
    if(bid < INODE_BITMAP_START){
        panic("bcache tries to write back superblock");
        return 1;
    }

    if(bid < INODE_START){
        panic("bcache tries to write back inode bitmap block");
        return 1;
    }

    if(bid < DATA_START){
        if(bp->type != BLK_TP_INODE && bp->type != BLK_TP_BITMAP){
            panic("bcache: block type does not match bid");
            return 1;
        }

        // no need to update hash in sb
        // sb_lock();
        // if(0 != sha256_block(bp->data, SB_HASH_PTR(bid))){
        //     sb_unlock();
        //     return 1;
        // }
        // sb_unlock();

    }else{
        if(bp->type != BLK_TP_DATA){
            panic("bcache: block type does not match bid");
            return 1;
        }

        pd_wb_lock();

        struct pd_wb_node *node = pd_wb_find(bid);
        if(node == NULL){
            if(0 != pd_wb_insert(bid)) goto error;
        }

        if(0 != sha256_block(bp->data, &node->hash)) goto error;

        memcpy(&node->hashidx, bp->hashidx, sizeof(bp->hashidx));

        pd_wb_unlock();
    }

    if(0 != aes128_block_encrypt(&bp->aes_iv, &bp->aes_key, bp->data))
        return 1;

    if(0 != disk_write(bp->data, bid))
        return 1;

    free(bp);
    return 0;

error:
    pd_wb_unlock();
    return 1;
}

int block_init(void)
{
    return cache_init(&bcac, bcache_cb_write) || map_init(&pd_wb);
}

static block_t *get_block_from_disk(uint32_t bid, uint16_t type,
                                    const hashidx_t hashidx[4], const key128_t *iv,
                                    const key128_t *key, const key256_t *exp_hash)
{
    block_t *bp = (block_t *)malloc(sizeof(block_t));
    if(bp == NULL) return NULL;

    if(0 != disk_read(bp->data, bid)) goto error;

    if(0 != aes128_block_decrypt(iv, key, bp->data)) goto error;

    if(0 != sha256_validate(bp->data, exp_hash)){
        char buf[51] = {0};
        snprintf(buf, 50, "block hash validation failed, bid = %d", bid);
        panic(buf);
        goto error;
    }

    bp->bid = bid;
    bp->type = type;
    if(type == BLK_TP_DATA){
        if(hashidx == NULL) goto error;
        memcpy(bp->hashidx, hashidx, sizeof(hashidx[0])*4);
    }
    memcpy(&bp->aes_iv, iv, sizeof(key128_t));
    memcpy(&bp->aes_key, key, sizeof(key128_t));

    return bp;

error:
    free(bp);
    return NULL;
}

int block_lock(uint32_t bid)
{
    return cache_node_lock(&bcac, bid);
}

int block_unlock(uint32_t bid)
{
    return cache_node_unlock(&bcac, bid);
}

block_t *bget_from_cache_lock(uint32_t bid, const hashidx_t hashidx[4], const key128_t *iv,
                                const key128_t *key, const key256_t *exp_hash)
{
    block_t *ret = cache_try_get(&bcac, bid, 1);
    if(ret) return ret;

    block_t **bpp = (block_t **)cache_insert_get(&bcac, bid, 1);

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

    if(!(*bpp)){
        if(type == BLK_TP_INODE || type == BLK_TP_BITMAP){
            sb_lock();
            *bpp = get_block_from_disk(bid, type, NULL, SB_IV_PTR(bid),
                                        SB_KEY_PTR(bid), SB_HASH_PTR(bid));
            sb_unlock();
        }else{
            *bpp = get_block_from_disk(bid, type, hashidx, iv, key, exp_hash);
            return *bpp;
        }
    }

    return *bpp;
}

int breturn_to_cache(uint32_t bid)
{
    return cache_return(&bcac, bid);
}

int block_unlock_return(block_t *bp)
{
    return cache_unlock_return(&bcac, bp->bid);
}

int block_make_dirty(uint32_t bid)
{
    return cache_make_dirty(&bcac, bid);
}

int pd_wb_lock(void)
{
    return pthread_mutex_lock(&pd_wb_lk);
}

int pd_wb_unlock(void)
{
    return pthread_mutex_unlock(&pd_wb_lk);
}

struct pd_wb_node *pd_wb_find(uint32_t bid)
{
    return map_search(&pd_wb, bid);
}

int pd_wb_insert(uint32_t bid)
{
    struct pd_wb_node *node = (struct pd_wb_node *)malloc(sizeof(struct pd_wb_node));

    return map_insert(&pd_wb, bid, node);
}

int pd_wb_delete(uint32_t bid)
{
    struct pd_wb_node *tofree = NULL;
    if(0 != map_delete(&pd_wb, bid, (void **)&tofree))
        free(tofree);

    return 0;
}

uint32_t block_alloc(void)
{
    return DID2BID(dbm_alloc());
}

int block_free(uint32_t bid, int setzero)
{
    if(bid < DATA_START)
        panic("block: try to free metadata blocks");

    // rm in pd_wb
    if(0 != pd_wb_delete(bid)) return 1;

    // find in cache and make deleted, may fail for not in cache
    cache_make_deleted(&bcac, bid);

    // dbm_free
    if(setzero) disk_setzero(bid);

    return dbm_free(BID2DID(bid));
}

// without hash validation, for writing back deleted inode only
uint8_t *bget_raw(uint32_t bid, const key128_t *iv, const key128_t *key, int *in_cache)
{
    if(bid < DATA_START) return NULL;

    block_t *ret = cache_try_get(&bcac, bid, 1);
    if(ret){
        *in_cache = 1;
        return ret->data;
    }

    uint8_t *blk = (uint8_t *)malloc(BLK_SZ);
    if(blk == NULL) return NULL;

    if(0 != disk_read(blk, bid)) return NULL;

    if(0 != aes128_block_decrypt(iv, key, blk)) return NULL;

    *in_cache = 0;

    return blk;
}

void breturn_raw(uint32_t bid, int in_cache, uint8_t *data)
{
    if(in_cache)
        cache_unlock_return(&bcac, bid);
    else
        free(data);
    return;
}

static int bget_simple(uint32_t bid, uint8_t *data, const key128_t *iv, const key128_t *key)
{
    if(0 != disk_read(data, bid)) return 1;

    if(0 != aes128_block_decrypt(iv, key, data)) return 1;

    return 0;
}

static void breturn_simple(uint32_t bid, uint8_t *data, const key128_t *iv, const key128_t *key)
{
    if(0 != aes128_block_encrypt(iv, key, data)) return;

    disk_write(data, bid);
}

int block_exit(void)
{
    cache_exit(&bcac);

    uint32_t id;
    struct pd_wb_node *node = NULL;
    key256_t hash;
    uint8_t data[BLK_SZ] = {0}, idata[BLK_SZ] = {0};

    sb_lock();

    while((node = map_clear_iter(&pd_wb, &id))){
        // update hash according to hashidx
        uint32_t ibid = INODE_IID2BID(node->hashidx[0].iid);
        if(0 != bget_simple(ibid, idata, SB_IV_PTR(ibid), SB_KEY_PTR(ibid)))
            continue;
        dinode_t *dip = (dinode_t *)(idata + INODE_BLOCK_OFFSET(node->hashidx[0].iid));

        memcpy(&hash, &node->hash, sizeof(key256_t));
        for(int i = 3; i > 0; i--){
            if(0 != bget_simple(node->hashidx[i].bid, data, &dip->aes_iv, &dip->aes_key))
                continue;

            index_t *idxp = (index_t *)(data + node->hashidx[i].idx * sizeof(index_t));
            memcpy(&idxp->hash, &hash, sizeof(key256_t));

            sha256_block(data, &hash);

            breturn_simple(node->hashidx[i].bid, data, &dip->aes_iv, &dip->aes_key);
        }

        memcpy(&dip->hash[node->hashidx[0].idx], &hash, sizeof(key256_t));
        breturn_simple(ibid, idata, SB_IV_PTR(ibid), SB_KEY_PTR(ibid));
        // no need to update hash in sb

        free(node);
    }

    sb_unlock();

    map_exit(&pd_wb);

    return 0;
}
