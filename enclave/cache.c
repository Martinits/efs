#include "types.h"
#include "queue.h"
#include "cache.h"
#include "enclave_t.h"
#include <stdlib.h>

static int node_free(struct cache *cac, struct list *node)
{
    if(node == NULL) return 1;

    if(node->dirty)
        if(0 != cac->content_cb(node->content, node->id, CONTENT_WRITE))
            return 1;

    free(node);
    return 0;
}

static struct list *content_is_in_cache(struct cache *cac, uint32_t id)
{
    // whether its in lru_list
    struct list *res = queue_search(&cac->lru, id);
    if(res){
        struct list *tofree = NULL;
        if(0 != queue_move_to_head(&cac->lru, res, (void **)&tofree))
            return NULL;
        if(tofree != NULL)
            if(0 != node_free(cac, tofree))
                return NULL;
        return res;
    }

    //whether its in fifo_list
    res = queue_search(&cac->fifo, id);
    if(res){
        struct list *tofree = NULL;
        if(0 != queue_evict(&cac->fifo, res))
            return NULL;
        if(0 != queue_insert_to_head(&cac->lru, res, (void **)&tofree))
            return NULL;
        if(tofree != NULL)
            if(0 != node_free(cac, tofree))
                return NULL;
        return res;
    }

    return NULL;
}

static struct list *content_put_in_cache(struct cache *cac, uint32_t id)
{
    struct list *res = content_is_in_cache(cac, id);
    if(res != NULL) return res;

    //not in cache
    res = (struct list *)malloc(sizeof(struct list));
    if(res == NULL) return NULL;

    res->id = id;
    res->dirty = 0;
    res->next = res->prev = NULL;
    res->refcnt = 0;

    if(0 != cac->content_cb(res->content, res->id, CONTENT_READ)){
        free(res);
        return NULL;
    }

    struct list *tofree = NULL;
    if(0 != queue_insert_to_head(&cac->fifo, res, (void **)&tofree)){
        free(res);
        return NULL;
    }
    if(tofree != NULL)
        if(0 != node_free(cac, tofree))
            return NULL;
    return res;
}

int cache_init(struct cache *cac, content_free_cb_t content_cb)
{
    cac->content_cb = content_cb;
    return queue_init(&cac->fifo) || queue_init(&cac->lru);
}

void *cache_get(struct cache *cac, uint32_t id, int rw)
{
    if(rw != CACHE_GET_RO && rw != CACHE_GET_RW)
        return NULL;

    struct list *node = content_put_in_cache(cac, id);
    if(node == NULL) return NULL;

    if(rw == CACHE_GET_RO){
        if(node->refcnt < 0) return NULL;
        node->refcnt++;
    }else{
        if(node->refcnt != 0) return NULL;
        node->refcnt = -1;
        node->dirty = 1;
    }

    return node->content;
}

int cache_return(struct cache *cac, uint32_t id, int rw)
{
    if(rw != CACHE_GET_RO && rw != CACHE_GET_RW)
        return 1;

    struct list *res = content_is_in_cache(cac, id);
    if(res == NULL) return 1;

    if(rw ==CACHE_GET_RO){
        if(res->refcnt <= 0) return 1;
        res->refcnt--;
    }else{
        if(res->refcnt != -1) return 1;
        res->refcnt = 0;
    }

    return 0;
}
