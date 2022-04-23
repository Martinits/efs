#include "types.h"
#include "queue.h"
#include "cache.h"
#include "error.h"
#include <stdlib.h>

static int node_free(cache_t *cac, struct list *node)
{
    if(node == NULL) return 1;

    if(node->dirty)
        if(0 != cac->content_cb_write(node->content, node->id))
            return 1;

    free(node);
    return 0;
}

static struct list *cache_find(cache_t *cac, uint32_t id)
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

static int cache_lock(cache_t *cac)
{
    return pthread_mutex_lock(&cac->lock);
}

static int cache_unlock(cache_t *cac)
{
    return pthread_mutex_unlock(&cac->lock);
}

int cache_init(cache_t *cac, content_cb_write_t content_cb_write)
{
    cac->lock = (typeof(cac->lock))PTHREAD_MUTEX_INITIALIZER;
    cac->content_cb_write = content_cb_write;
    return queue_init(&cac->fifo) || queue_init(&cac->lru);
}

int cache_is_in(cache_t *cac, uint32_t id)
{
    cache_lock(cac);
    struct list *res = cache_find(cac, id);
    cache_unlock(cac);
    return res == NULL ? 0 : 1;
}

int cache_insert(cache_t *cac, uint32_t id, void *content)
{
    struct list *res = (struct list *)malloc(sizeof(struct list));
    if(res == NULL) return 1;

    res->id = id;
    res->dirty = 0;
    res->next = res->prev = NULL;
    res->refcnt = 0;
    res->content = content;

    cache_lock(cac);

    struct list *tofree = NULL;
    if(0 != queue_insert_to_head(&cac->fifo, res, (void **)&tofree)){
        free(res);
        cache_unlock(cac);
        return 1;
    }
    if(tofree != NULL){
        if(0 != node_free(cac, tofree)){
            cache_unlock(cac);
            return 1;
        }
    }

    return 0;
}

void *cache_try_get(cache_t *cac, uint32_t id)
{
    cache_lock(cac);

    struct list *res = cache_find(cac, id);
    if(res == NULL){
        cache_unlock(cac);
        return NULL;
    }

    res->refcnt++;

    cache_unlock(cac);

    return res->content;
}

void *cache_insert_get(cache_t *cac, uint32_t id, void *content)
{
    if(0 != cache_insert(cac, id, content))
        return NULL;

    return cache_try_get(cac, id);

}

int cache_make_dirty(cache_t *cac, uint32_t id)
{
    cache_lock(cac);

    struct list *res = cache_find(cac, id);
    if(res == NULL){
        cache_unlock(cac);
        return 1;
    }

    res->dirty = 1;

    cache_unlock(cac);

    return 0;
}

int cache_return(cache_t *cac, uint32_t id)
{
    cache_lock(cac);

    struct list *res = cache_find(cac, id);
    if(res == NULL){
        cache_unlock(cac);
        return 1;
    }

    if(res->refcnt <= 0)
        panic("cache return 0 refcnt block");

    res->refcnt--;

    cache_unlock(cac);

    return 0;
}
