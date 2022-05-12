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

extern cache_t icac;
cache_t bcac;

struct map pd_wb;
uint32_t pd_wb_len = 0;
pthread_mutex_t pd_wb_lk = PTHREAD_MUTEX_INITIALIZER;


// not holding cache lock now
// no need to lock block because refcnt == 0 so no other is holding it
static int bcache_cb_write(void *content, uint32_t bid, int dirty, int deleted)
{
    block_t *bp = (block_t *)content;

    if(deleted){
        pd_wb_lock();
        pd_wb_delete(bid);
        pd_wb_unlock();
        free(bp);
        return 0;
    }

    if(!dirty){
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

        // must have hold sb lock
        //sb_lock();
        if(0 != sha256_block(bp->data, SB_HASH_PTR(bid))){
            sb_unlock();
            return 1;
        }
        //sb_unlock();

    }else{
        if(bp->type != BLK_TP_DATA){
            panic("bcache: block type does not match bid");
            return 1;
        }

        key256_t hash;
        if(0 != sha256_block(bp->data, &hash)) goto error;

        int incache = 0;

        /*int i = 3;*/
        /*while(i > 0 && bp->hashidx[i].bid == 0) i--;*/
        /*if(i > 0){*/
            /*// write hash to index*/
            /*block_t *ret = cache_try_get(&bcac, bp->hashidx[i].bid, 0, 0);*/
            /*if(ret){*/
                /*incache = 1;*/
                /*index_t *idxp = (index_t *)(ret->data) + bp->hashidx[i].idx;*/
                /*memcpy(&idxp->hash, &hash, sizeof(hash));*/
                /*if(0 != cache_dirty_unlock_return(&bcac, bp->hashidx[i].bid))*/
                    /*goto error;*/
            /*}*/
        /*}*/

        // if upper is inode, do not try to get from icac, may cause deadlock
        /*else{*/
            /*// i == 0, write hash to inode*/
            /*inode_t *ip = cache_try_get(&icac, bp->hashidx[0].iid, 1, 0);*/
            /*if(ip){*/
                /*incache = 1;*/
                /*memcpy(&ip->hash[bp->hashidx[0].idx], &hash, sizeof(hash));*/
                /*if(0 != cache_dirty_unlock_return(&icac, bp->hashidx[0].iid)) goto error;*/
            /*}*/
        /*}*/

        pd_wb_lock();

        if(incache){
            pd_wb_delete(bid);
        }else{
            struct pd_wb_node *node = pd_wb_find(bid);
            if(node == NULL){
                node = pd_wb_insert(bid);
                if(node == NULL) goto pd_wb_error;
                memcpy(node->hashidx, bp->hashidx, sizeof(bp->hashidx));
            }
            memcpy(&node->hash, &hash, sizeof(hash));
        }

        pd_wb_unlock();
    }

    if(0 != aes128_block_encrypt(&bp->aes_iv, &bp->aes_key, bp->data))
        return 1;

    if(0 != disk_write(bp->data, bid))
        return 1;

    free(bp);
    return 0;

pd_wb_error:
    pd_wb_unlock();
error:
    panic("bcache_cb_write failed");
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
    if(bp == NULL){
        panic("malloc failed");
        return NULL;
    }

    if(exp_hash){
        if(0 != disk_read(bp->data, bid)) goto error;

        if(0 != aes128_block_decrypt(iv, key, bp->data)) goto error;

        if(0 != sha256_validate(bp->data, exp_hash)){
            panic("block hash validation failed, bid = %d, type = %d, \
                    hashidx = {%d, %d}, {%d, %d}, {%d, %d}, {%d, %d}", bid, type,
                    hashidx[0].iid, hashidx[0].idx,
                    hashidx[1].bid, hashidx[1].idx,
                    hashidx[2].bid, hashidx[2].idx,
                    hashidx[3].bid, hashidx[3].idx);
            goto error;
        }
    }else{
        // new allocated block
        memset(bp->data, 0, BLK_SZ);

        // char buf[100] = {0};
        // for(uint i=0;i<sizeof(iv->k);i++) snprintf(buf+2*i, 3, "%02x", iv->k[i]);
        // elog(LOG_DEBUG, "new block %d: iv = %s", bid, buf);

        // for(uint i=0;i<sizeof(key->k);i++) snprintf(buf+2*i, 3, "%02x", key->k[i]);
        // elog(LOG_DEBUG, "new block %d: key = %s", bid, buf);
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
    block_t *ret = cache_try_get(&bcac, bid, 1, 1);
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
            // no need to sb_lock since the caller should have take care of it
            *bpp = get_block_from_disk(bid, type, NULL, iv, key, exp_hash);
        }else{
            *bpp = get_block_from_disk(bid, type, hashidx, iv, key, exp_hash);
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

struct pd_wb_node *pd_wb_insert(uint32_t bid)
{
    struct pd_wb_node *node = (struct pd_wb_node *)malloc(sizeof(struct pd_wb_node));
    if(node == NULL){
        panic("malloc failed");
        return NULL;
    }

    if(0 != map_insert(&pd_wb, bid, node)){
        free(node);
        return NULL;
    }

    pd_wb_len++;
    // elog(LOG_DEBUG, "pd_wb_len = %d", pd_wb_len);

    return node;
}

int pd_wb_delete(uint32_t bid)
{
    struct pd_wb_node *tofree = NULL;
    if(0 != map_delete(&pd_wb, bid, (void **)&tofree))
        return 1;

    free(tofree);
    pd_wb_len--;

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

    block_t *ret = cache_try_get(&bcac, bid, 1, 1);
    if(ret){
        *in_cache = 1;
        return ret->data;
    }

    uint8_t *blk = (uint8_t *)malloc(BLK_SZ);
    if(blk == NULL){
        panic("malloc failed");
        return NULL;
    }

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

static int breturn_simple(uint32_t bid, uint8_t *data, const key128_t *iv, const key128_t *key)
{
    if(0 != aes128_block_encrypt(iv, key, data)) return 1;

    if(0 != disk_write(data, bid)) return 1;

    return 0;
}

int block_exit(void)
{
    elog(LOG_DEBUG, "block exit");

    if(0 != cache_exit(&bcac)) return 1;

    uint32_t id;
    struct pd_wb_node *node = NULL;
    key256_t hash;
    uint8_t data[BLK_SZ] = {0}, idata[BLK_SZ] = {0};

    sb_lock();

    // deduplicate
    elog(LOG_DEBUG, "deduplication");

    struct map dupbids;
    if(0 != map_init(&dupbids)) return 1;

    int dummy;
    while((node = map_clear_iter(&pd_wb, &id))){
        int i = 3;
        while(i > 0 && node->hashidx[i].bid == 0) i--;
        for(; i > 0; i--)
            map_insert(&dupbids, node->hashidx[i].bid, &dummy);
    }

    while(&dummy == map_clear_iter(&dupbids, &id)){
        pd_wb_delete(id);
    }

    if(0 != map_exit(&dupbids)) return 1;


    // merge
    elog(LOG_DEBUG, "merge");

    // write back cascadedly
    elog(LOG_DEBUG, "clear pd_wb %d", pd_wb_len);

    while((node = map_clear_iter(&pd_wb, &id))){
        // update hash according to hashidx
        uint32_t ibid = INODE_IID2BID(node->hashidx[0].iid);
        if(0 != bget_simple(ibid, idata, SB_IV_PTR(ibid), SB_KEY_PTR(ibid)))
            return 1;
        dinode_t *dip = (dinode_t *)(idata + INODE_BLOCK_OFFSET(node->hashidx[0].iid));

        memcpy(&hash, &node->hash, sizeof(key256_t));

        int i = 3;
        while(i > 0 && node->hashidx[i].bid == 0) i--;
        for(; i > 0; i--){
            if(0 != bget_simple(node->hashidx[i].bid, data, &dip->aes_iv, &dip->aes_key))
                return 1;

            index_t *idxp = (index_t *)(data) + node->hashidx[i].idx;
            memcpy(&idxp->hash, &hash, sizeof(key256_t));

            if(0 != sha256_block(data, &hash)) return 1;

            if(0 != breturn_simple(node->hashidx[i].bid, data, &dip->aes_iv, &dip->aes_key))
                return 1;
        }

        memcpy(&dip->hash[node->hashidx[0].idx], &hash, sizeof(key256_t));
        if(0 != breturn_simple(ibid, idata, SB_IV_PTR(ibid), SB_KEY_PTR(ibid)))
            return 1;
        // no need to update hash in sb

        free(node);
    }

    sb_unlock();

    map_exit(&pd_wb);

    elog(LOG_DEBUG, "block exit done");

    return 0;
}
