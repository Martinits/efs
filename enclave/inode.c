#include "types.h"
#include "inode.h"
#include "cache.h"
#include "security.h"
#include "layout.h"
#include "error.h"
#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern superblock_t sb;

cache_t bcac, icac;

static int icache_cb_write(void *content, uint32_t id)
{

}

static int bcache_cb_write(void *content, uint32_t id)
{

}

int inode_init(void){
    return cache_init(&bcac, bcache_cb_write) || cache_init(&icac, icache_cb_write);
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

static int block_lock(block_t *bp)
{
    return pthread_mutex_lock(&bp->lock);
}

// must hold block lock
static int block_unlock(block_t *bp)
{
    return pthread_mutex_unlock(&bp->lock);
}

static block_t *bget_from_cache(uint32_t bid, uint16_t iid,
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

static block_t *bget_from_cache_lock(uint32_t bid, uint16_t iid,
                                        const uint8_t hash_subidx[4], const key128_t *iv,
                                        const key128_t *key, const key256_t *exp_hash)
{
    block_t *bp = bget_from_cache(bid, iid, hash_subidx, iv, key, exp_hash);
    if(bp == NULL) return NULL;

    block_lock(bp);

    return bp;
}

static int breturn_to_cache(uint32_t bid)
{
    return cache_return(&bcac, bid);
}

// must hold block lock
static int block_unlock_return(block_t *bp)
{
    uint32_t bid = bp->bid;
    return block_unlock(bp) || breturn_to_cache(bid);
}


static inode_t *get_inode_from_disk(uint16_t iid)
{
    block_t *bp = bget_from_cache_lock(INODE_DISK_OFFSET(iid), 0, NULL, NULL, NULL, NULL);
    if(bp == NULL) return NULL;

    dinode_t *dip = (dinode_t *)(bp->data + INODE_BLOCK_OFFSET(iid));

    inode_t *ip = (inode_t *)malloc(sizeof(inode_t));
    if(ip == NULL) return NULL;

    ip->iid = iid;
    ip->type = dip->type;
    ip->size = dip->size;
    memcpy(ip->bid, &dip->bid, sizeof(ip->bid));
    ip->lock = (typeof(ip->lock))PTHREAD_MUTEX_INITIALIZER;
    ip->aes_key = dip->aes_key;
    ip->aes_iv = dip->aes_iv;
    memcpy(ip->hash, &dip->hash, sizeof(ip->hash));

    block_unlock(bp);

    return ip;
}

static int inode_lock(inode_t *ip)
{
    return pthread_mutex_lock(&ip->lock);
}

// must hold inode lock
static int inode_unlock(inode_t *ip)
{
    return pthread_mutex_unlock(&ip->lock);
}

static inode_t *iget_from_cache(uint16_t iid)
{
    if(cache_is_in(&icac, iid))
        return cache_try_get(&icac, iid);

    inode_t *ip = get_inode_from_disk(iid);
    return cache_insert_get(&icac, iid, (void *)ip);
}

static int ireturn_to_cache(uint16_t iid)
{
    return cache_return(&icac, iid) || breturn_to_cache(INODE_DISK_OFFSET(iid));
}

// must hold inode lock
static int inode_unlock_return(inode_t *ip)
{
    uint16_t iid = ip->iid;
    return inode_unlock(ip) || ireturn_to_cache(iid);
}

static void index_lookup_one_level(index_t *idxp, inode_t *ip, uint32_t bid, uint8_t bidx,
                                    const uint8_t hash_subidx[4], const key256_t *exp_hash)
{
    block_t *bp = bget_from_cache_lock(bid, ip->iid, hash_subidx,
                                &ip->aes_iv, &ip->aes_key, exp_hash);
    if(bp == NULL) return;

    memcpy(idxp, bp->data + bidx * sizeof(index_t), sizeof(index_t));
    block_unlock_return(bp);

    return;
}

// must hold inode lock
static uint32_t inode_index_lookup(inode_t *ip, uint32_t bidx,
                                    uint8_t hash_subidx[4], key256_t *exp_hash)
{
    block_t *bp = NULL;
    index_t idx;

    hash_subidx = (uint8_t [4]){0, 0, 0, 0};

    if(bidx < NDIRECT){
        hash_subidx[0] = (uint8_t)bidx;
        exp_hash = &ip->hash[NDIRECT];
        return ip->bid[bidx];
    }
    bidx -= NDIRECT;

    if(bidx < NINDIRECT1){
        hash_subidx[0] = NDIRECT;
        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT], (uint8_t)bidx,
                                hash_subidx, &ip->hash[NDIRECT]);

        hash_subidx[1] = (uint8_t)bidx;
        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT1;

    if(bidx < NINDIRECT2){
        hash_subidx[0] = NDIRECT + 1;
        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 1],
                                (uint8_t)(bidx/INDEX_PER_BLOCK),
                                hash_subidx, &ip->hash[NDIRECT + 1]);

        hash_subidx[1] = (uint8_t)(bidx /= INDEX_PER_BLOCK);
        index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx, hash_subidx, &idx.hash);

        hash_subidx[2] = (uint8_t)bidx;
        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT2;

    hash_subidx[0] = NDIRECT + 2;
    index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 2],
                            (uint8_t)(bidx/INDEX_PER_BLOCK/INDEX_PER_BLOCK),
                            hash_subidx, &ip->hash[NDIRECT + 1]);

    hash_subidx[1] = (uint8_t)(bidx /= INDEX_PER_BLOCK * INDEX_PER_BLOCK);
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)(bidx/INDEX_PER_BLOCK),
                            hash_subidx, &idx.hash);

    hash_subidx[2] = (uint8_t)(bidx /= INDEX_PER_BLOCK);
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx, hash_subidx, &idx.hash);
    memcpy(exp_hash, &idx.hash, sizeof(key256_t));
    return idx.bid;
}

// must hold inode lock
static int inode_read_data(inode_t *ip, void *dst, uint32_t off, uint32_t size)
{
    if(off >= ip->size || off + size <= off)
        return 0;

    if(off + size >= ip->size)
        size = ip->size - off;

    uint32_t red = 0, round = 0;
    uint8_t *to = (uint8_t *)dst;
    block_t *bp;
    uint8_t hash_subidx[4] = {0};
    key256_t exp_hash;

    for(red = 0; red < size; red += round, to += round, off += round){
        uint32_t bid = inode_index_lookup(ip, off/BLK_SZ, hash_subidx, &exp_hash);
        bp = bget_from_cache_lock(bid, ip->iid, hash_subidx,
                                    &ip->aes_iv, &ip->aes_key, &exp_hash);

#define MIN(a, b) ((a) > (b) ? (b) : (a))
        round = MIN(size - red, BLK_SZ - off%BLK_SZ);
        memcpy(to, bp->data + off%BLK_SZ, round);
        block_unlock_return(bp);
    }

    return red;
}

// must hold inode lock
static uint16_t inode_getsubdir(inode_t *ip, const char *dirname)
{
    dirent_t de;

    for(uint32_t off = 0; off < ip->size; off += sizeof(dirent_t)){
        if(inode_read_data(ip, &de, off, sizeof(de)) != sizeof(de))
            panic("dirread failed");
        if(de.iid == 0)
            continue;
        if(!strcmp(dirname, de.name))
            return de.iid;
    }

    return 0;
}

static const char *name_of_next_level(const char *path, char *buf)
{
    int i = 0;
    while(path[i] != 0 && path[i] != '/') i++;

    if(i > DIRNAME_MAX_LEN) return NULL;

    memcpy(buf, path, i);
    buf[i] = 0;

    return path[i] == 0 ? (path + i) : (path + i + 1);
}

inode_t *inode_get(const char *path)
{
    if(path[0] != '/') return NULL;

    const char *p = path + 1;
    inode_t *ip = iget_from_cache(sb.rootinode), *tmp = NULL;
    char buf[DIRNAME_MAX_LEN+1] = {0};

    while(*p){
        p = name_of_next_level(p, buf);
        if(buf[0] == 0) break;

        inode_lock(ip);

        if(ip->type != INODE_TP_DIR) goto error;

        uint16_t iid = inode_getsubdir(ip, buf);
        if(iid == 0) goto error;

        inode_unlock_return(ip);

        tmp = iget_from_cache(iid);
        if(tmp == NULL) goto error;

        ip = tmp;
    }

    return ip;

error:
    inode_unlock_return(ip);
    return NULL;
}

int inode_read(inode_t *ip, uint8_t *to, uint32_t offset, uint32_t len)
{

}

int inode_write(inode_t *ip, uint8_t *from, uint32_t offset, uint32_t len)
{

}

int inode_return(inode_t *ip)
{

}
