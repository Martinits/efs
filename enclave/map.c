#include "types.h"
#include "map.h"
#include <stdlib.h>

struct treenode {
    struct rb_node node;
    uint32_t blk_id;
    struct block *pos_in_list;
};

static struct treenode *rb_search(struct rb_root *root, uint32_t blk_id)
{
    struct rb_node *node = root->rb_node;

    while(node){
        struct treenode *data = container_of(node, struct treenode, node);

        if(blk_id < data->blk_id)
            node = node->rb_left;
        else if(blk_id > data->blk_id)
            node = node->rb_right;
        else
            return data;
    }
    return NULL;
}

static int rb_insert(struct rb_root *root, struct treenode *data)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while(*new){
        struct treenode *this = container_of(*new, struct treenode, node);

        parent = *new;
        if(data->blk_id < this->blk_id)
            new = &((*new)->rb_left);
        else if(data->blk_id > this->blk_id)
            new = &((*new)->rb_right);
        else
            return 1;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    return 0;
}

static void rb_free(struct treenode *node)
{
    if(node == NULL) return;
    free(node);
}

int map_init(struct map *mp)
{
    mp->tree_root = RB_ROOT;

    return 0;
}

int map_insert(struct map *mp, uint32_t blk_id, struct block* pos)
{
    struct treenode *newnode = (struct treenode *)malloc(sizeof(struct treenode));
    if(newnode == NULL) return 1;

    newnode->blk_id = blk_id;
    newnode->pos_in_list = pos;

    return rb_insert(&mp->tree_root, newnode);
}

struct block *map_search(struct map *mp, uint32_t blk_id)
{
    struct treenode *result = rb_search(&mp->tree_root, blk_id);
    if(result == NULL) return NULL;

    return result->pos_in_list;
}

int map_delete(struct map *mp, uint32_t blk_id)
{
    struct treenode *result = rb_search(&mp->tree_root, blk_id);
    if(result == NULL) return 1;

    rb_erase(&result->node, &mp->tree_root);
    rb_free(result);
    return 0;
}
