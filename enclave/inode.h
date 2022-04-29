#ifndef _INODE_H
#define _INODE_H

#include "types.h"
#include "layout.h"
#include "security.h"
#include <pthread.h>

#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define INODE_TP_DIR (0)
#define INODE_TP_FILE (1)
typedef struct {
    uint16_t iid; // 0 for empty
    uint32_t type;
    uint32_t size;
    dinode_t *dip;
    uint32_t bid[NDIRECT + 3]; //0 for empty
    key128_t aes_key, aes_iv;
    key256_t hash[NDIRECT + 3];
} inode_t;

int inode_init(void);

int inode_lock(inode_t *ip);

int inode_unlock(inode_t *ip);

inode_t *inode_get(const char *path);

int inode_read(inode_t *ip, uint8_t *to, uint32_t offset, uint32_t len);

int inode_write(inode_t *ip, uint8_t *from, uint32_t offset, uint32_t len);

int inode_return(inode_t *ip);

#endif
