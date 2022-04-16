#include "types.h"
#include "queue.h"
#include "cache.h"
#include "error.h"
#include <stdlib.h>

static int node_free(struct cache *cac, struct list *node)
{
    if(node == NULL) return 1;

    if(node->dirty)
        if(0 != cac->content_cb_write(node->content, node->id))
            return 1;

    free(node);
    return 0;
}

static struct list *cache_find(struct cache *cac, uint32_t id)
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

int cache_init(struct cache *cac, content_cb_write_t content_cb_write)
{
    cac->content_cb_write = content_cb_write;
    return queue_init(&cac->fifo) || queue_init(&cac->lru);
}

int cache_is_in(struct cache *cac, uint32_t id)
{
    return cache_find(cac, id) == NULL ? 0 : 1;
}

int cache_insert(struct cache *cac, uint32_t id, void *content)
{
    struct list *res = (struct list *)malloc(sizeof(struct list));
    if(res == NULL) return 1;

    res->id = id;
    res->dirty = 0;
    res->next = res->prev = NULL;
    res->refcnt = 0;
    res->content = content;

    struct list *tofree = NULL;
    if(0 != queue_insert_to_head(&cac->fifo, res, (void **)&tofree)){
        free(res);
        return 1;
    }
    if(tofree != NULL)
        if(0 != node_free(cac, tofree))
            return 1;
    return 0;
}

void *cache_try_get(struct cache *cac, uint32_t id)
{
    struct list *res = cache_find(cac, id);
    if(res == NULL) return NULL;

    res->refcnt++;

    return res->content;
}

int cache_make_dirty(struct cache *cac, uint32_t id)
{
    struct list *res = cache_find(cac, id);
    if(res == NULL) return 1;

    res->dirty = 1;

    return 0;
}

int cache_return(struct cache *cac, uint32_t id)
{
    struct list *res = cache_find(cac, id);
    if(res == NULL) return 1;

    if(res->refcnt <= 0)
        panic("cache return 0 refcnt block");

    res->refcnt--;

    return 0;
}
