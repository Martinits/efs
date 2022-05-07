#include "types.h"
#include "inode.h"
#include "cache.h"
#include "security.h"
#include "layout.h"
#include "error.h"
#include "disk.h"
#include "block.h"
#include "bitmap.h"
#include "superblock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern superblock_t sb;

cache_t icac;

static void index_free_recursive(uint32_t bid, int level, const key128_t *iv, const key128_t *key)
{
    int in_cache;
    index_t *idxp = (index_t *)bget_raw(bid, iv, key, &in_cache);

    for(uint i = 0; i < INDEX_PER_BLOCK; i++){
        if(idxp[i].bid){
            if(level <= 1) block_free(idxp[i].bid, 0);
            else index_free_recursive(idxp[i].bid, level-1, iv, key);
        }
    }

    breturn_raw(bid, in_cache, (uint8_t *)idxp);
    block_free(bid, 1);
}

// not holding cache lock now
// no need to lock ip because refcnt == 0 so no other is holding it
static int icache_cb_write(void *content, uint32_t iid, int dirty, int deleted)
{
    inode_t *ip = (inode_t *)content;
    uint32_t bid = INODE_IID2BID(iid);

    if(deleted){
        // go through index and block_free()
        // index block free with setzero
        for(int i = 0; i < NDIRECT; i++){
            if(ip->bid[i])
                block_free(ip->bid[i], 0);
        }

        if(ip->bid[NDIRECT])
            index_free_recursive(ip->bid[NDIRECT], 1, &ip->aes_iv, &ip->aes_key);

        if(ip->bid[NDIRECT + 1])
            index_free_recursive(ip->bid[NDIRECT + 1], 2, &ip->aes_iv, &ip->aes_key);

        if(ip->bid[NDIRECT + 2])
            index_free_recursive(ip->bid[NDIRECT + 2], 3, &ip->aes_iv, &ip->aes_key);

        // setzero inode block
        block_lock(bid);

        memset(ip->dip, 0, sizeof(dinode_t));

        block_make_dirty(bid);
        block_unlock(bid);

        // ibm_free
        ibm_free((uint16_t)iid);

    }else if(dirty){
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
    free(ip);
    return 0;
}

int inode_init(void)
{
    return cache_init(&icac, icache_cb_write);
}

static int inode_lock(inode_t *ip)
{
    return cache_node_unlock(&icac, ip->iid);
}

static int inode_unlock(inode_t *ip)
{
    return cache_node_lock(&icac, ip->iid);
}

static inode_t *iget_from_cache(uint16_t iid, int sure_not_cached)
{
    if(!sure_not_cached){
        inode_t *ret = cache_try_get(&icac, iid, 0);
        if(ret) return ret;
    }

    inode_t **ipp = (inode_t **)cache_insert_get(&icac, iid, 0);

    uint32_t ibid = INODE_IID2BID(iid);

    sb_lock();
    block_t *bp = bget_from_cache_lock(INODE_IID2BID(iid), NULL,
                                        SB_IV_PTR(ibid), SB_KEY_PTR(ibid), SB_HASH_PTR(ibid));
    sb_unlock();
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

static int inode_make_deleted(inode_t *ip)
{
    return cache_make_deleted(&icac, ip->iid);
}

static void get_real_hash_ip(inode_t *ip, uint32_t bidx)
{
    pd_wb_lock();
    struct pd_wb_node *node = pd_wb_find(ip->bid[bidx]);
    if(node){
        memcpy(&ip->hash[bidx], &node->hash, sizeof(key256_t));
        pd_wb_delete(ip->bid[bidx]);
        inode_make_dirty(ip);
    }
    pd_wb_unlock();
}

static void get_real_hash_index(index_t *idxp, uint32_t bid)
{
    pd_wb_lock();
    struct pd_wb_node *node = pd_wb_find(idxp->bid);
    if(node){
        memcpy(&idxp->hash, &node->hash, sizeof(key256_t));
        pd_wb_delete(idxp->bid);
        block_make_dirty(bid);
    }
    pd_wb_unlock();
}

static void index_lookup_one_level(index_t *idxp, inode_t *ip, uint32_t bid,
                                    uint8_t bidx, const hashidx_t hashidx[4],
                                    const key256_t *exp_hash, int write, int *new)
{
    block_t *bp = bget_from_cache_lock(bid, hashidx,
                                &ip->aes_iv, &ip->aes_key, exp_hash);
    if(bp == NULL) return;

    index_t *idxp_tmp = (index_t *)(bp->data) + bidx;

    *new = 0;

    if(write && idxp_tmp->bid == 0){
        idxp_tmp->bid = block_alloc();
        if(idxp_tmp->bid == 0) return;
        *new = 1;
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
                                    hashidx_t hashidx[4], key256_t *exp_hash, int *new)
{
    index_t idx;

    *new = 0;

    hashidx[0].iid = ip->iid;

    if(bidx < NDIRECT){
        hashidx[0].idx = (uint8_t)bidx;

        if(write && ip->bid[bidx] == 0){
            ip->bid[bidx] = block_alloc();
            if(ip->bid[bidx] == 0) return 0;
            *new = 1;
            memcpy(&ip->hash[bidx], zero_block_sha256(), sizeof(key256_t));
            inode_make_dirty(ip);
        }else{
            get_real_hash_ip(ip, bidx);
        }

        memcpy(exp_hash, &ip->hash[bidx], sizeof(key256_t));
        return ip->bid[bidx];
    }
    bidx -= NDIRECT;

    if(bidx < NINDIRECT1){
        hashidx[0].idx = NDIRECT;

        int tmpnew = 0;
        if(write && ip->bid[NDIRECT] == 0){
            ip->bid[NDIRECT] = block_alloc();
            if(ip->bid[NDIRECT] == 0) return 0;
            tmpnew = 1;
            memcpy(&ip->hash[NDIRECT], zero_block_sha256(), sizeof(key256_t));
            inode_make_dirty(ip);
        }else{
            get_real_hash_ip(ip, NDIRECT);
        }

        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT], (uint8_t)bidx,
                                hashidx, tmpnew ? NULL : &ip->hash[NDIRECT], write, new);
        hashidx[1].bid = ip->bid[NDIRECT];
        hashidx[1].idx = (uint8_t)bidx;

        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT1;

    if(bidx < NINDIRECT2){
        hashidx[0].idx = NDIRECT + 1;

        int tmpnew = 0;
        if(write && ip->bid[NDIRECT + 1] == 0){
            ip->bid[NDIRECT + 1] = block_alloc();
            if(ip->bid[NDIRECT + 1] == 0) return 0;
            tmpnew = 1;
            memcpy(&ip->hash[NDIRECT + 1], zero_block_sha256(), sizeof(key256_t));
            inode_make_dirty(ip);
        }else{
            get_real_hash_ip(ip, NDIRECT + 1);
        }

        index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 1],
                                (uint8_t)(bidx/INDEX_PER_BLOCK), hashidx,
                                tmpnew ? NULL : &ip->hash[NDIRECT + 1], write, &tmpnew);
        hashidx[1].bid = ip->bid[NDIRECT + 1];
        hashidx[1].idx = (uint8_t)(bidx / INDEX_PER_BLOCK);

        bidx %= INDEX_PER_BLOCK;

        index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx,
                                hashidx, tmpnew ? NULL : &idx.hash, write, new);
        hashidx[2].bid = idx.bid;
        hashidx[2].idx = (uint8_t)bidx;

        memcpy(exp_hash, &idx.hash, sizeof(key256_t));
        return idx.bid;
    }
    bidx -= NINDIRECT2;

    hashidx[0].idx = NDIRECT + 2;

    int tmpnew = 0;
    if(write && ip->bid[NDIRECT + 2] == 0){
        ip->bid[NDIRECT + 2] = block_alloc();
        if(ip->bid[NDIRECT + 2] == 0) return 0;
        tmpnew = 1;
        memcpy(&ip->hash[NDIRECT + 2], zero_block_sha256(), sizeof(key256_t));
        inode_make_dirty(ip);
    }else{
        get_real_hash_ip(ip, NDIRECT + 2);
    }

    index_lookup_one_level(&idx, ip, ip->bid[NDIRECT + 2],
                            (uint8_t)(bidx/INDEX_PER_BLOCK/INDEX_PER_BLOCK),
                            hashidx, tmpnew ? NULL : &ip->hash[NDIRECT + 2], write, &tmpnew);
    hashidx[1].bid = ip->bid[NDIRECT + 2];
    hashidx[1].idx = (uint8_t)(bidx / INDEX_PER_BLOCK / INDEX_PER_BLOCK);

    bidx %= INDEX_PER_BLOCK * INDEX_PER_BLOCK;
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)(bidx/INDEX_PER_BLOCK),
                            hashidx, tmpnew ? NULL : &idx.hash, write, &tmpnew);
    hashidx[2].bid = idx.bid;
    hashidx[2].idx = (uint8_t)(bidx / INDEX_PER_BLOCK);

    bidx %= INDEX_PER_BLOCK;
    index_lookup_one_level(&idx, ip, idx.bid, (uint8_t)bidx, hashidx,
                            tmpnew ? NULL : &idx.hash, write, new);
    hashidx[3].bid = idx.bid;
    hashidx[3].idx = (uint8_t)bidx;

    memcpy(exp_hash, &idx.hash, sizeof(key256_t));
    return idx.bid;
}

// must hold inode lock
static uint32_t inode_rw_data(inode_t *ip, void *buf, uint32_t off, uint32_t size, int write)
{
    if(off + size <= off)
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
    hashidx_t hashidx[4];
    key256_t exp_hash;
    int new;

    memset(hashidx, 0, sizeof(hashidx));

    for(done = 0; done < size; done += round, tofrom += round, off += round){
        uint32_t bid = inode_index_lookup(ip, off/BLK_SZ, write, hashidx, &exp_hash, &new);

        bp = bget_from_cache_lock(bid, hashidx, &ip->aes_iv, &ip->aes_key, new ? NULL : &exp_hash);

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
static uint16_t inode_getsub(inode_t *ip, const char *dirname)
{
    dirent_t de;

    for(uint32_t off = 0; off < ip->size; off += sizeof(dirent_t)){
        if(sizeof(de) != inode_rw_data(ip, &de, off, sizeof(de), 0))
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

// must hold ip lock
static inode_t *inode_new_get(inode_t *ip, const char *name, uint16_t type)
{
    //find whether same name
    dirent_t de;
    int empty = -1;

    if(strlen(name) > sizeof(de.name)) return NULL;

    for(uint32_t off = 0; off < ip->size; off += sizeof(dirent_t)){
        if(sizeof(de) != inode_rw_data(ip, &de, off, sizeof(de), 0))
            panic("dirread failed");
        if(de.iid == 0){
            if(empty == -1) empty = off / sizeof(de);
            continue;
        }
        if(!strcmp(name, de.name))
            return NULL;
    }

    if(empty == -1)
        empty = ip->size / sizeof(de);

    uint16_t iid = ibm_alloc();
    if(iid == 0) return 0;

    de.iid = iid;
    strncpy(de.name, name, sizeof(de.name));

    if(sizeof(de) != inode_rw_data(ip, &de, empty * (uint32_t)sizeof(de), sizeof(de), 1))
        return NULL;

    inode_t *newip = iget_from_cache(iid, 1);
    inode_lock(newip);

    newip->iid = iid;
    newip->type = type;
    newip->size = 0;
    memset(&newip->bid, 0, sizeof(newip->bid));
    memset(&newip->hash, 0, sizeof(ip->hash));
    key128_gen(&ip->aes_iv);
    key128_gen(&ip->aes_key);
    inode_make_dirty(newip);

    inode_unlock(newip);
    return newip;
}

// must hold ip lock
static uint16_t inode_new(inode_t *ip, const char *name, uint16_t type)
{
    inode_t *newip = inode_new_get(ip, name, type);
    if(newip == NULL) return 0;

    uint16_t iid = newip->iid;
    inode_return(newip);

    return iid;
}

// must hold ip lock
static int inode_delete(inode_t *ip, const char *name, uint16_t type)
{
    //whether exist
    dirent_t de;
    inode_t *delip = NULL;

    if(strlen(name) > sizeof(de.name)) return 1;

    for(uint32_t off = 0; off < ip->size; off += sizeof(dirent_t)){
        if(sizeof(de) != inode_rw_data(ip, &de, off, sizeof(de), 0))
            panic("dirread failed");
        if(de.iid == 0)
            continue;
        if(!strcmp(name, de.name))
            goto found;
    }

    return 1;

found:
    delip = iget_from_cache(de.iid, 0);
    if(delip == NULL) return 1;

    inode_lock(delip);
    if(delip->type != type){
        inode_unlock(delip);
        return 1;
    }
    inode_make_deleted(delip);
    inode_unlock(delip);

    return 0;
}

// return not locked ip
static inode_t *inode_get(const char *path, uint16_t type, int alloc)
{
    if(type == INODE_TP_DIR && alloc) return NULL;

    if(path[0] != '/') return NULL;

    const char *p = path + 1;
    inode_t *ip = iget_from_cache(sb.rootinode, 0), *tmp = NULL;
    char buf[DIRNAME_MAX_LEN+1] = {0};

    while(*p){
        p = name_of_next_level(p, buf);
        if(buf[0] == 0) break;

        inode_lock(ip);

        if(ip->type != INODE_TP_DIR) goto error;

        uint16_t iid = inode_getsub(ip, buf);
        if(iid == 0){
            if(*p == 0 && alloc)
                iid = inode_new(ip, buf, INODE_TP_FILE);
            else goto error;
        }

        inode_unlock_return(ip);

        tmp = iget_from_cache(iid, 0);
        if(tmp == NULL) goto error;

        ip = tmp;
    }

    inode_lock(ip);
    if(ip->type != type) goto error;
    inode_unlock(ip);

    return ip;

error:
    inode_unlock_return(ip);
    return NULL;
}

inode_t *inode_get_file(const char *path, int alloc)
{
    return inode_get(path, INODE_TP_FILE, alloc);
}

inode_t *inode_get_dir(const char *path)
{
    return inode_get(path, INODE_TP_DIR, 0);
}

uint32_t inode_read_file(inode_t *ip, uint8_t *to, uint32_t offset, uint32_t len)
{
    if(ip == NULL) return 0;

    uint32_t ret = 0;
    inode_lock(ip);

    if(ip->type == INODE_TP_FILE)
        ret = inode_rw_data(ip, to, offset, len, 0);

    inode_unlock(ip);

    return ret;
}

uint32_t inode_write_file(inode_t *ip, uint8_t *from, uint32_t offset, uint32_t len)
{
    if(ip == NULL) return 0;

    uint32_t ret = 0;
    inode_lock(ip);

    if(ip->type == INODE_TP_FILE)
        ret = inode_rw_data(ip, from, offset, len, 1);

    inode_unlock(ip);

    return ret;
}

int inode_mkdir(inode_t *ip, const char *name)
{
    inode_lock(ip);
    uint16_t iid = inode_new(ip, name, INODE_TP_DIR);
    inode_unlock(ip);

    return iid == 0;
}

int inode_rmdir(inode_t *ip, const char *name)
{
    inode_lock(ip);
    int ret = inode_delete(ip, name, INODE_TP_DIR);
    inode_unlock(ip);

    return ret;
}

// int inode_mkfile(inode_t *ip, const char *name)
// {
//     inode_lock(ip);
//     uint16_t iid = inode_new(ip, name, INODE_TP_FILE);
//     inode_unlock(ip);
// 
//     return iid == 0;
// }

int inode_rmfile(inode_t *ip, const char* name)
{
    inode_lock(ip);
    int ret = inode_delete(ip, name, INODE_TP_FILE);
    inode_unlock(ip);

    return ret;
}

int inode_return(inode_t *ip)
{
    return cache_return(&icac, ip->iid);
}

uint32_t inode_get_size(inode_t *ip)
{
    inode_lock(ip);
    uint32_t ret = ip->size;
    inode_unlock(ip);

    return ret;
}

int inode_exit(void)
{
    return cache_exit(&icac);
}
