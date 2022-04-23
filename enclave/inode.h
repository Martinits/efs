#ifndef _INODE_H
#define _INODE_H

#include "types.h"
#include "layout.h"
#include "security.h"
#include <pthread.h>

#define INODE_TP_DIR (0)
#define INODE_TP_FILE (1)
typedef struct {
    uint16_t iid; // 0 for empty
    uint32_t type;
    uint32_t size;
    uint32_t bid[NDIRECT + 3]; //0 for empty
    pthread_mutex_t lock;
    key128_t aes_key, aes_iv;
    key256_t hash[NDIRECT + 3];
} inode_t;

#define BLK_TP_INODE  (0)
#define BLK_TP_BITMAP (1)
#define BLK_TP_DATA   (2)
// #define BLK_TP_INDEX  (3)
#define HASH_IS_IN_INODE (0)
#define HASH_IS_IN_INDEX (1)
typedef struct {
    uint16_t type;
    uint16_t iid;
    uint8_t hash_subidx[4];
    uint32_t bid;
    pthread_mutex_t lock;
    uint8_t data[BLK_SZ];
} block_t;

int inode_init(void);

inode_t *inode_get(const char *path);

int inode_read(inode_t *ip, uint8_t *to, uint32_t offset, uint32_t len);

int inode_write(inode_t *ip, uint8_t *from, uint32_t offset, uint32_t len);

int inode_return(inode_t *ip);

#endif
