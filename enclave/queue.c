#include "types.h"
#include "queue.h"

int queue_init(struct queue *q)
{
    return map_init(&q->mp);
}

int queue_evict(struct queue *q, struct block *evict)
{
    if(q->head.list_len == 0) return 1;

    evict->prev->next = evict->next;
    evict->next->prev = evict->prev;
    q->head.list_len--;

    if(map_delete(&q->mp, evict->blk_id) != 0) return 1;

    evict->prev = evict->next = NULL;

    return 0;
}

struct block *queue_dieout(struct queue *q)
{
    if(q->head.list_len == 0) return NULL;

    struct block *evict = q->head.prev;
    while(evict != &q->head && evict->refcnt != 0) evict = evict->prev;
    if(evict == &q->head) return NULL;

    if(queue_evict(q, evict) != 0)
        return NULL;

    return evict;
}

int queue_insert_to_head(struct queue *q, struct block *node, struct block **write_back)
{
    *write_back = NULL;

    if(q->head.list_len >= QUEUE_MAX_LEN){
        struct block *ret = queue_dieout(q);
        if(ret == NULL) return 1;
        *write_back = ret;
    }

    q->head.next->prev = node;
    node->next = q->head.next;
    q->head.next = node;
    node->prev = &q->head;
    q->head.list_len++;

    return map_insert(&q->mp, node->blk_id, node);
}

int queue_move_to_head(struct queue *q, struct block *node, struct block **write_back)
{
    return queue_evict(q, node) || queue_insert_to_head(q, node, write_back);
}

struct block *queue_search(struct queue *q, uint32_t blk_id)
{
    if(q->head.list_len == 0) return NULL;

    return map_search(&q->mp, blk_id);
}
