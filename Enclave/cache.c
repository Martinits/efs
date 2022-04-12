#include "types.h"
#include "cache.h"
#include <stdlib.h>

struct cache cac;

static int queue_evict(struct queue *q, struct block *evict)
{
    if(q->head.list_len == 0) return 1;

    evict->prev->next = evict->next;
    evict->next->prev = evict->prev;
    q->head.list_len--;

    if(map_delete(&cac.lru.mp, evict->blk_id) != 0) return 1;

    evict->prev = evict->next = NULL;

    return 0;
}

static int queue_node_free(struct block *node)
{
    if(node->dirty){
        ocall_disk_write();
    }

    free(node);

    return 0;
}

static int queue_dieout(struct queue *q)
{
    if(q->head.list_len == 0) return 1;

    struct block *evict = q->head.prev;
    while(evict != &q->head && evict->refcnt != 0) evict = evict->prev;
    if(evict == &q->head) return 1;

    return queue_evict(q, evict) || queue_node_free(evict);
}

static int queue_insert_to_head(struct queue *q, struct block *node)
{
    if(q->head.list_len >= QUEUE_MAX_LEN){
        if(queue_dieout(q) != 0) return 1;
    }

    q->head.next->prev = node;
    node->next = q->head.next;
    q->head.next = node;
    node->prev = &q->head;
    q->head.list_len++;

    return map_insert(&q->mp, node->blk_id, node);
}

static int queue_fifo_move_to_lru_head(struct block *node)
{
    return queue_evict(&cac.fifo, node) || queue_insert_to_head(&cac.lru, node);
}


static int queue_move_to_head(struct queue *q, struct block *node)
{
    return queue_evict(q, node) || queue_insert_to_head(q, node);
}

static struct block *queue_search(struct queue *q, uint32_t blk_id)
{
    if(q->head.list_len == 0) return NULL;

    return map_search(&q->mp, blk_id);
}

int cache_init(void)
{
    return map_init(&cac.fifo.mp) || map_init(&cac.lru.mp);
}

static struct block *block_is_in_cache(uint32_t id)
{
    // whether its in lru_list
    struct block *res = queue_search(&cac.lru, id);
    if(res){
        if(!queue_move_to_head(&cac.lru, res))
            return NULL;
        return res;
    }

    //whether its in fifo_list
    res = queue_search(&cac.fifo, id);
    if(res){
        if(!queue_evict(&cac.fifo, res)) return NULL;
        if(!queue_insert_to_head(&cac.lru, res)) return NULL;
        return res;
    }

    return NULL;
}

static struct block *block_put_in_cache(uint32_t id)
{
    struct block *res = block_is_in_cache(id);
    if(res != NULL) return res;

    //not in cache
    res = (struct block *)malloc(sizeof(struct block));
    if(res == NULL) return NULL;

    res->blk_id = id;
    res->dirty = 0;
    res->next = res->prev = NULL;
    res->refcnt = 0;

    ocall_disk_read();

    if(queue_insert_to_head(&cac.fifo, res) != 0)
        return NULL;
    return res;
}

uint8_t *block_get_ro(uint32_t id)
{
    struct block *blk = block_put_in_cache(id);
    if(blk == NULL) return NULL;

    if(blk->refcnt < 0) return NULL;
    blk->refcnt++;

    return blk->data;
}

uint8_t *block_get_rw(uint32_t id)
{
    struct block *blk = block_put_in_cache(id);
    if(blk == NULL) return NULL;

    if(blk->refcnt != 0) return NULL;
    blk->refcnt = -1;
    blk->dirty = 1;

    return blk->data;
}

int block_return_ro(uint32_t id)
{
    struct block *res = block_is_in_cache(id);
    if(res == NULL) return 1;

    if(res->refcnt <= 0) return 1;
    res->refcnt--;

    return 0;
}

int block_return_rw(uint32_t id)
{
    struct block *res = block_is_in_cache(id);
    if(res == NULL) return 1;

    if(res->refcnt != -1) return 1;
    res->refcnt = 0;

    return 0;
}

int block_pin(uint32_t id)
{
    return 0;
}
