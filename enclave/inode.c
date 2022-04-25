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

cache_t icac;

static int icache_cb_write(void *content, uint32_t id)
{

}

int inode_init(void)
{
    return block_init() || cache_init(&icac, icache_cb_write);
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

static int inode_make_dirty(inode_t *ip)
{
    return cache_make_dirty(&icac, ip->iid);
}

static void index_lookup_one_level(index_t *idxp, inode_t *ip, uint32_t bid,
                                    uint8_t bidx, const uint8_t hash_subidx[4],
                                    const key256_t *exp_hash, int write)
{
    block_t *bp = bget_from_cache_lock(bid, ip->iid, hash_subidx,
                                &ip->aes_iv, &ip->aes_key, exp_hash);
    if(bp == NULL) return;

    index_t *idxp_tmp = (index_t *)(bp->data + bidx * sizeof(index_t));

    if(write && idxp_tmp->bid == 0){
        idxp_tmp->bid = block_alloc();
        if(ip->bid[bidx] == 0) return;
        memcpy(&idxp_tmp->hash, zero_block_sha256(), sizeof(key256_t));
        block_make_dirty(bp);
    }

    memcpy(idxp, idxp_tmp, sizeof(index_t));
    block_unlock_return(bp);

    return;
}

// must hold inode lock
static uint32_t inode_index_lookup(inode_t *ip, uint32_t bidx, int write,
                                    uint8_t hash_subidx[4], key256_t *exp_hash)
{
    index_t idx;

    memset(hash_subidx, 0, sizeof(hash_subidx[0]) * 4);

    if(bidx < NDIRECT){
        hash_subidx[0] = (uint8_t)bidx;

        if(write && ip->bid[bidx] == 0){
            ip->bid[bidx] = block_alloc();
            if(ip->bid[bidx] == 0) return 0;
            memcpy(&ip->hash[bidx], zero_block_sha256(), sizeof(key256_t));
            inode_make_dirty(ip);
        }

        memcpy(exp_hash, &ip->hash[bidx], sizeof(key256_t));
        return ip->bid[bidx];
    }
    bidx -= NDIRECT;

    if(bidx < NINDIRECT1){
        hash_subidx[0] = NDIRECT;
        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT], (uint8_t)bidx,
                                hash_subidx, &ip->hash[NDIRECT], write);

        hash_subidx[1] = (uint8_t)bidx;
        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT1;

    if(bidx < NINDIRECT2){
        hash_subidx[0] = NDIRECT + 1;
        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 1],
                                (uint8_t)(bidx/INDEX_PER_BLOCK), hash_subidx,
                                &ip->hash[NDIRECT + 1], write);

        hash_subidx[1] = (uint8_t)(bidx /= INDEX_PER_BLOCK);
        index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx,
                                hash_subidx, &idx.hash, write);

        hash_subidx[2] = (uint8_t)bidx;
        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT2;

    hash_subidx[0] = NDIRECT + 2;
    index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 2],
                            (uint8_t)(bidx/INDEX_PER_BLOCK/INDEX_PER_BLOCK),
                            hash_subidx, &ip->hash[NDIRECT + 1], write);

    hash_subidx[1] = (uint8_t)(bidx /= INDEX_PER_BLOCK * INDEX_PER_BLOCK);
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)(bidx/INDEX_PER_BLOCK),
                            hash_subidx, &idx.hash, write);

    hash_subidx[2] = (uint8_t)(bidx /= INDEX_PER_BLOCK);
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx, hash_subidx, &idx.hash, write);
    memcpy(exp_hash, &idx.hash, sizeof(key256_t));
    return idx.bid;
}

// must hold inode lock
static uint32_t inode_rw_data(inode_t *ip, void *buf, uint32_t off, uint32_t size, int write)
{
    if(off >= ip->size || off + size <= off)
        return 0;

    if(write){
        if(off + size >= UINT32_MAX)
            return 0;
    }else{
        if(off + size >= ip->size)
            size = ip->size - off;
    }

    uint32_t done = 0, round = 0;
    uint8_t *tofrom = (uint8_t *)buf;
    block_t *bp;
    uint8_t hash_subidx[4] = {0};
    key256_t exp_hash;

    for(done = 0; done < size; done += round, tofrom += round, off += round){
        uint32_t bid = inode_index_lookup(ip, off/BLK_SZ, write, hash_subidx, &exp_hash);
        bp = bget_from_cache_lock(bid, ip->iid, hash_subidx,
                                    &ip->aes_iv, &ip->aes_key, &exp_hash);

        round = MIN(size - done, BLK_SZ - off%BLK_SZ);
        if(write){
            memcpy(bp->data + off%BLK_SZ, tofrom, round);
            block_make_dirty(bp);
        }else{
            memcpy(tofrom, bp->data + off%BLK_SZ, round);
        }
        block_unlock_return(bp);
    }

    if(write){
        if(off > ip->size)
            ip->size = off;
        inode_make_dirty(ip);
    }

    return done;
}

// must hold inode lock
static uint16_t inode_getsubdir(inode_t *ip, const char *dirname)
{
    dirent_t de;

    for(uint32_t off = 0; off < ip->size; off += sizeof(dirent_t)){
        if(inode_rw_data(ip, &de, off, sizeof(de), 0) != sizeof(de))
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
    if(ip == NULL) return 0;

    inode_lock(ip);
    uint32_t ret = inode_rw_data(ip, to, offset, len, 0);
    inode_unlock(ip);

    return ret;
}

int inode_write(inode_t *ip, uint8_t *from, uint32_t offset, uint32_t len)
{
    if(ip == NULL) return 0;

    inode_lock(ip);
    uint32_t ret = inode_rw_data(ip, from, offset, len, 1);
    inode_unlock(ip);

    return ret;
}

int inode_return(inode_t *ip)
{
    return ireturn_to_cache(ip->iid);
}
