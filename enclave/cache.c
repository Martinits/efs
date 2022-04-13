#include "types.h"
#include "queue.h"
#include "cache.h"
#include "enclave_t.h"
#include <stdlib.h>

static int node_free(struct cache *cac, struct block *node)
{
    if(node == NULL) return 1;

    if(node->dirty)
        if(0 != cac->block_rw(node->data, node->blk_id, BLOCK_WRITE))
            return 1;

    free(node);
    return 0;
}

static struct block *block_is_in_cache(struct cache *cac, uint32_t id)
{
    // whether its in lru_list
    struct block *res = queue_search(&cac->lru, id);
    if(res){
        struct block *tofree = NULL;
        if(0 != queue_move_to_head(&cac->lru, res, &tofree))
            return NULL;
        if(tofree != NULL)
            if(0 != node_free(cac, tofree))
                return NULL;
        return res;
    }

    //whether its in fifo_list
    res = queue_search(&cac->fifo, id);
    if(res){
        struct block *tofree = NULL;
        if(0 != queue_evict(&cac->fifo, res))
            return NULL;
        if(0 != queue_insert_to_head(&cac->lru, res, &tofree))
            return NULL;
        if(tofree != NULL)
            if(0 != node_free(cac, tofree))
                return NULL;
        return res;
    }

    return NULL;
}

static struct block *block_put_in_cache(struct cache *cac, uint32_t id)
{
    struct block *res = block_is_in_cache(cac, id);
    if(res != NULL) return res;

    //not in cache
    res = (struct block *)malloc(sizeof(struct block));
    if(res == NULL) return NULL;

    res->blk_id = id;
    res->dirty = 0;
    res->next = res->prev = NULL;
    res->refcnt = 0;

    if(0 != cac->block_rw(res->data, res->blk_id, BLOCK_READ)){
        free(res);
        return NULL;
    }

    struct block *tofree = NULL;
    if(0 != queue_insert_to_head(&cac->fifo, res, &tofree)){
        free(res);
        return NULL;
    }
    if(tofree != NULL)
        if(0 != node_free(cac, tofree))
            return NULL;
    return res;
}

int cache_init(struct cache *cac, blockio_callback_t block_rw)
{
    cac->block_rw = block_rw;
    return queue_init(&cac->fifo) || queue_init(&cac->lru);
}

uint8_t *cache_get_block(struct cache *cac, uint32_t blk_id, int rw)
{
    if(rw != BLOCK_GET_RO && rw != BLOCK_GET_RW)
        return NULL;

    struct block *blk = block_put_in_cache(cac, blk_id);
    if(blk == NULL) return NULL;

    if(rw == BLOCK_GET_RO){
        if(blk->refcnt < 0) return NULL;
        blk->refcnt++;
    }else{
        if(blk->refcnt != 0) return NULL;
        blk->refcnt = -1;
        blk->dirty = 1;
    }

    return blk->data;
}

int cache_return_block(struct cache *cac, uint32_t blk_id, int rw)
{
    if(rw != BLOCK_GET_RO && rw != BLOCK_GET_RW)
        return 1;

    struct block *res = block_is_in_cache(cac, blk_id);
    if(res == NULL) return 1;

    if(rw ==BLOCK_GET_RO){
        if(res->refcnt <= 0) return 1;
        res->refcnt--;
    }else{
        if(res->refcnt != -1) return 1;
        res->refcnt = 0;
    }

    return 0;
}
