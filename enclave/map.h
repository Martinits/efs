#ifndef _MAP_H
#define _MAP_H

#include "rbtree.h"
#include "types.h"

struct map {
    struct rb_root tree_root;
};

int map_init(struct map *mp);

int map_insert(struct map *mp, uint32_t id, void *data);

void *map_search(struct map *mp, uint32_t id);

int map_delete(struct map *mp, uint32_t id, void **tofree);

void *map_clear_iter(struct map *mp, uint32_t *id);

int map_exit(struct map *mp);

#endif
