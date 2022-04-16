#ifndef _MAP_H
#define _MAP_H

#include "rbtree.h"
#include "types.h"

struct list;

struct map {
    struct rb_root tree_root;
};

int map_init(struct map *mp);

int map_insert(struct map *mp, uint32_t id, struct list *pos);

struct list *map_search(struct map *mp, uint32_t id);

int map_delete(struct map *mp, uint32_t id);

#endif
