#ifndef _QUEUE_H
#define _QUEUE_H

#include "types.h"
#include "map.h"
#include <pthread.h>

#define QUEUE_MAX_LEN (256)

struct list {
    uint32_t id;
    uint32_t refcnt;
    int dirty;
    void *content;
    struct list *prev;
    struct list *next;
    pthread_mutex_t lock;
};

struct queue {
    struct list head;
    struct map mp;
    uint32_t len;
};

int queue_init(struct queue *q);

int queue_evict(struct queue *q, struct list *evict);

struct list *queue_dieout(struct queue *q);

int queue_insert_to_head(struct queue *q, struct list *node, void **drop);

int queue_move_to_head(struct queue *q, struct list *node, void **drop);

struct list *queue_search(struct queue *q, uint32_t id);

#endif
