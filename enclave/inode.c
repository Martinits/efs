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

static int icache_cb_write(void *content, uint32_t iid, int dirty)
{
    inode_t *ip = (inode_t *)content;
    uint32_t bid = INODE_IID2BID(iid);

    if(dirty){
        block_lock(bid);

        ip->dip->type = ip->type;
        ip->dip->size = ip->size;
        memcpy(ip->dip->bid, &ip->bid, sizeof(ip->bid));
        ip->dip->aes_key = ip->aes_key;
        ip->dip->aes_iv = ip->aes_iv;
        memcpy(ip->dip->hash, &ip->hash, sizeof(ip->hash));

        block_make_dirty(bid);
        block_unlock(bid);
    }

    breturn_to_cache(bid);
    free(content);
    return 0;
}

int inode_init(void)
{
    return block_init() || cache_init(&icac, icache_cb_write);
}

int inode_lock(inode_t *ip)
{
    return cache_node_unlock(&icac, ip->iid);
}

int inode_unlock(inode_t *ip)
{
    return cache_node_lock(&icac, ip->iid);
}

static inode_t *iget_from_cache(uint16_t iid)
{
    inode_t *ret = cache_try_get(&icac, iid, 0);
    if(ret) return ret;

    inode_t **ipp = (inode_t **)cache_insert_get(&icac, iid, 0);

    block_t *bp = bget_from_cache_lock(INODE_IID2BID(iid), /*NULL,*/ NULL, NULL, NULL);
    if(bp == NULL) return NULL;

    dinode_t *dip = (dinode_t *)(bp->data + INODE_BLOCK_OFFSET(iid));

    inode_t *ip = (inode_t *)malloc(sizeof(inode_t));
    if(ip == NULL) return NULL;

    ip->iid = iid;
    ip->type = dip->type;
    ip->size = dip->size;
    memcpy(ip->bid, &dip->bid, sizeof(ip->bid));
    ip->aes_key = dip->aes_key;
    ip->aes_iv = dip->aes_iv;
    memcpy(ip->hash , &dip->hash, sizeof(ip->hash));
    ip->dip = dip;

    block_unlock(bp->bid);

    return *ipp = ip;
}

static int inode_unlock_return(inode_t *ip)
{
    return cache_unlock_return(&icac, ip->iid);
}

static int inode_make_dirty(inode_t *ip)
{
    return cache_make_dirty(&icac, ip->iid);
}

static void get_real_hash_ip(inode_t *ip, uint32_t bidx)
{
    pd_wb_lock();
    key256_t *real_hash = pd_wb_find(ip->bid[bidx]);
    if(real_hash){
        memcpy(&ip->hash[bidx], real_hash, sizeof(key256_t));
        pd_wb_delete(ip->bid[bidx]);
        inode_make_dirty(ip);
    }
    pd_wb_unlock();
}

static void get_real_hash_index(index_t *idxp, uint32_t bid)
{
    pd_wb_lock();
    key256_t *real_hash = pd_wb_find(idxp->bid);
    if(real_hash){
        memcpy(&idxp->hash, real_hash, sizeof(key256_t));
        pd_wb_delete(idxp->bid);
        block_make_dirty(bid);
    }
    pd_wb_unlock();
}

static void index_lookup_one_level(index_t *idxp, inode_t *ip, uint32_t bid,
                                    uint8_t bidx, /* const hashidx_t hashidx[4],*/
                                    const key256_t *exp_hash, int write)
{
    block_t *bp = bget_from_cache_lock(bid, /*hashidx,*/
                                &ip->aes_iv, &ip->aes_key, exp_hash);
    if(bp == NULL) return;

    index_t *idxp_tmp = (index_t *)(bp->data + bidx * sizeof(index_t));

    if(write && idxp_tmp->bid == 0){
        idxp_tmp->bid = block_alloc();
        if(idxp_tmp->bid == 0) return;
        memcpy(&idxp_tmp->hash, zero_block_sha256(), sizeof(key256_t));
        block_make_dirty(bp->bid);
    }else{
        get_real_hash_index(idxp_tmp, bid);
    }

    memcpy(idxp, idxp_tmp, sizeof(index_t));
    block_unlock_return(bp);

    return;
}

// must hold inode lock
static uint32_t inode_index_lookup(inode_t *ip, uint32_t bidx, int write,
                                    /*hashidx_t hashidx[4],*/ key256_t *exp_hash)
{
    index_t idx;

    /*memset(hashidx, 0, sizeof(hashidx[0]) * 4);*/

    /*hashidx[0].iid = ip->iid;*/

    if(bidx < NDIRECT){
        /*hashidx[0].idx = (uint8_t)bidx;*/

        if(write && ip->bid[bidx] == 0){
            ip->bid[bidx] = block_alloc();
            if(ip->bid[bidx] == 0) return 0;
            memcpy(&ip->hash[bidx], zero_block_sha256(), sizeof(key256_t));
            inode_make_dirty(ip);
        }

        get_real_hash_ip(ip, bidx);

        memcpy(exp_hash, &ip->hash[bidx], sizeof(key256_t));
        return ip->bid[bidx];
    }
    bidx -= NDIRECT;

    if(bidx < NINDIRECT1){
        /*hashidx[0].idx = NDIRECT;*/

        get_real_hash_ip(ip, NDIRECT);

        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT], (uint8_t)bidx,
                                /*hashidx,*/ &ip->hash[NDIRECT], write);
        /*hashidx[1].bid = ip->bid[NDIRECT];*/
        /*hashidx[1].idx = (uint8_t)bidx;*/

        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT1;

    if(bidx < NINDIRECT2){
        /*hashidx[0].idx = NDIRECT + 1;*/

        get_real_hash_ip(ip, NDIRECT + 1);

        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 1],
                                (uint8_t)(bidx/INDEX_PER_BLOCK), /*hashidx,*/
                                &ip->hash[NDIRECT + 1], write);
        /*hashidx[1].bid = ip->bid[NDIRECT + 1];*/
        /*hashidx[1].idx = (uint8_t)(bidx / INDEX_PER_BLOCK);*/

        bidx %= INDEX_PER_BLOCK;

        index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx,
                                /*hashidx,*/ &idx.hash, write);
        /*hashidx[2].bid = idx.bid;*/
        /*hashidx[2].idx = (uint8_t)bidx;*/

        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT2;

    /*hashidx[0].idx = NDIRECT + 2;*/

    get_real_hash_ip(ip, NDIRECT + 2);

    index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 2],
                            (uint8_t)(bidx/INDEX_PER_BLOCK/INDEX_PER_BLOCK),
                            /*hashidx,*/ &ip->hash[NDIRECT + 2], write);
    /*hashidx[1].bid = ip->bid[NDIRECT + 2];*/
    /*hashidx[1].idx = (uint8_t)(bidx / INDEX_PER_BLOCK / INDEX_PER_BLOCK);*/

    bidx %= INDEX_PER_BLOCK * INDEX_PER_BLOCK;
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)(bidx/INDEX_PER_BLOCK),
                            /*hashidx,*/ &idx.hash, write);
    /*hashidx[2].bid = idx.bid;*/
    /*hashidx[2].idx = (uint8_t)(bidx / INDEX_PER_BLOCK);*/

    bidx %= INDEX_PER_BLOCK;
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx, /*hashidx,*/ &idx.hash, write);
    /*hashidx[3].bid = idx.bid;*/
    /*hashidx[3].idx = (uint8_t)bidx;*/

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
    /*hashidx_t hashidx[4];*/
    key256_t exp_hash;

    /*memset(hashidx, 0, sizeof(hashidx));*/

    for(done = 0; done < size; done += round, tofrom += round, off += round){
        uint32_t bid = inode_index_lookup(ip, off/BLK_SZ, write, /*hashidx,*/ &exp_hash);

        bp = bget_from_cache_lock(bid, /*hashidx,*/ &ip->aes_iv, &ip->aes_key, &exp_hash);

        round = MIN(size - done, BLK_SZ - off%BLK_SZ);
        if(write){
            memcpy(bp->data + off%BLK_SZ, tofrom, round);
            block_make_dirty(bp->bid);
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
    return cache_return(&icac, ip->iid);
}
