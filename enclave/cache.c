#include "types.h"
#include "queue.h"
#include "cache.h"
#include "error.h"
#include <stdlib.h>

static int node_free(content_cb_write_t content_cb_write, struct list *node)
{
    if(node == NULL) return 1;

    if(content_cb_write)
        if(0 != content_cb_write(node->content, node->id, node->dirty, node->deleted))
            return 1;

    free(node);
    return 0;
}

static int cache_lock(cache_t *cac)
{
    return pthread_mutex_lock(&cac->lock);
}

static int cache_unlock(cache_t *cac)
{
    return pthread_mutex_unlock(&cac->lock);
}

static struct list *cache_find(cache_t *cac, uint32_t id, int access)
{
    cache_lock(cac);

    // whether its in lru_list
    struct list *res = queue_search(&cac->lru, id);
    if(res){
        if(access){
            struct list *tofree = NULL;
            if(0 != queue_move_to_head(&cac->lru, res, (void **)&tofree))
                goto error;
            if(tofree != NULL)
                panic("cache: move to head yield queue overflow");
        }
        cache_unlock(cac);
        return res;
    }

    //whether its in fifo_list
    res = queue_search(&cac->fifo, id);
    if(res){
        struct list *tofree = NULL;
        if(access){
            if(0 != queue_evict(&cac->fifo, res))
                goto error;
            if(0 != queue_insert_to_head(&cac->lru, res, (void **)&tofree))
                goto error;
        }
        cache_unlock(cac);
        if(access && tofree != NULL)
            if(0 != node_free(cac->content_cb_write, tofree))
                panic("cannot write back node");
        return res;
    }

error:
    cache_unlock(cac);
    return NULL;
}

int cache_node_lock(cache_t *cac, uint32_t id)
{
    struct list *node = cache_find(cac, id, 0);
    if(node == NULL) return 1;

    return pthread_mutex_lock(&node->lock);
}

int cache_node_unlock(cache_t *cac, uint32_t id)
{
    struct list *node = cache_find(cac, id, 0);
    if(node == NULL) return 1;

    return pthread_mutex_unlock(&node->lock);
}

int cache_init(cache_t *cac, content_cb_write_t content_cb_write)
{
    cac->lock = (typeof(cac->lock))PTHREAD_MUTEX_INITIALIZER;
    cac->content_cb_write = content_cb_write;
    return queue_init(&cac->fifo) || queue_init(&cac->lru);
}

static int node_lock(struct list *node)
{
    return pthread_mutex_lock(&node->lock);
}

static int node_unlock(struct list *node)
{
    return pthread_mutex_unlock(&node->lock);
}

void **cache_insert_get(cache_t *cac, uint32_t id, int lock)
{
    struct list *res = (struct list *)malloc(sizeof(struct list));
    if(res == NULL){
        panic("malloc failed");
        return NULL;
    }

    res->id = id;
    res->dirty = 0;
    res->next = res->prev = NULL;
    res->refcnt = 1;
    res->deleted = 0;
    res->content = NULL;
    res->lock = (typeof(res->lock))PTHREAD_MUTEX_INITIALIZER;

    struct list *tofree = NULL;

    cache_lock(cac);
    if(0 != queue_insert_to_head(&cac->fifo, res, (void **)&tofree)){
        free(res);
        cache_unlock(cac);
        return NULL;
    }
    cache_unlock(cac);

    if(tofree != NULL){
        if(0 != node_free(cac->content_cb_write, tofree))
            panic("cannot write back node");
    }

    if(lock) node_lock(res);

    return &res->content;
}

void *cache_try_get(cache_t *cac, uint32_t id, int lock)
{
    struct list *res = cache_find(cac, id, 1);
    if(res == NULL) return NULL;

    node_lock(res);
    if(res->deleted){
        node_unlock(res);
        return NULL;
    }
    res->refcnt++;
    if(!lock) node_unlock(res);
    return res->content;
}

// must hold node lock
int cache_make_dirty(cache_t *cac, uint32_t id)
{
    struct list *node = cache_find(cac, id, 0);
    if(node == NULL) return 1;

    node->dirty = 1;

    return 0;
}

// must hold node lock
int cache_make_deleted(cache_t *cac, uint32_t id)
{
    struct list *node = cache_find(cac, id, 0);
    if(node == NULL) return 1;

    node->deleted = 1;

    return 0;
}

// must hold node lock
int cache_return(cache_t *cac, uint32_t id)
{
    struct list *node = cache_find(cac, id, 0);
    if(node == NULL) return 1;

    node_lock(node);

    if(node->refcnt <= 0)
        panic("cache return 0 refcnt block");
    node->refcnt--;

    node_unlock(node);

    return 0;
}

int cache_unlock_return(cache_t *cac, uint32_t id)
{
    struct list *node = cache_find(cac, id, 0);
    if(node == NULL) return 1;

    if(node->refcnt <= 0)
        panic("cache return 0 refcnt block");
    node->refcnt--;

    node_unlock(node);

    return 0;
}

int cache_exit(cache_t *cac)
{
    elog(LOG_INFO, "cache exit");

    struct list *p, *tmp;

    //free fifo
    p = cac->fifo.head.next;
    while(p != &cac->fifo.head){
        tmp = p->next;
        node_free(cac->content_cb_write, p);
        p = tmp;
    }

    //free lru
    p = cac->lru.head.next;
    while(p != &cac->lru.head){
        tmp = p->next;
        node_free(cac->content_cb_write, p);
        p = tmp;
    }

    return queue_exit(&cac->fifo) || queue_exit(&cac->lru);
}
