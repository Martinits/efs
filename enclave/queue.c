#include "types.h"
#include "queue.h"
#include "map.h"

int queue_init(struct queue *q)
{
    return map_init(&q->mp);
}

int queue_evict(struct queue *q, struct list *evict)
{
    if(q->len == 0) return 1;

    evict->prev->next = evict->next;
    evict->next->prev = evict->prev;
    q->len--;

    if(map_delete(&q->mp, evict->id, NULL) != 0) return 1;

    evict->prev = evict->next = NULL;

    return 0;
}

struct list *queue_dieout(struct queue *q)
{
    if(q->len == 0) return NULL;

    struct list *evict = q->head.prev;
    while(evict != &q->head && evict->refcnt != 0) evict = evict->prev;
    if(evict == &q->head) return NULL;

    if(queue_evict(q, evict) != 0)
        return NULL;

    return evict;
}

int queue_insert_to_head(struct queue *q, struct list *node, void **write_back)
{
    *write_back = NULL;

    if(q->len >= QUEUE_MAX_LEN){
        struct list *ret = queue_dieout(q);
        if(ret == NULL) return 1;
        *write_back = (void *)ret;
    }

    q->head.next->prev = node;
    node->next = q->head.next;
    q->head.next = node;
    node->prev = &q->head;
    q->len++;

    return map_insert(&q->mp, node->id, node);
}

int queue_move_to_head(struct queue *q, struct list *node, void **write_back)
{
    return queue_evict(q, node) || queue_insert_to_head(q, node, write_back);
}

struct list *queue_search(struct queue *q, uint32_t id)
{
    if(q->len == 0) return NULL;

    return map_search(&q->mp, id);
}

int queue_exit(struct queue *q)
{
    return map_exit(&q->mp);
}
