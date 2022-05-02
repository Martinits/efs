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
    uint16_t type;
    uint32_t size;
    dinode_t *dip;
    uint32_t bid[NDIRECT + 3]; //0 for empty
    key128_t aes_key, aes_iv;
    key256_t hash[NDIRECT + 3];
} inode_t;

int inode_init(void);

inode_t *inode_get_file(const char *path, int alloc);

inode_t *inode_get_dir(const char *path);

uint32_t inode_read_file(inode_t *ip, uint8_t *to, uint32_t offset, uint32_t len);

uint32_t inode_write_file(inode_t *ip, uint8_t *from, uint32_t offset, uint32_t len);

int inode_mkdir(inode_t *ip, const char *name);

int inode_rmdir(inode_t *ip, const char *name);

// int inode_mkfile(inode_t *ip, const char *name);

int inode_rmfile(inode_t *ip, const char* name);

int inode_return(inode_t *ip);

uint32_t inode_get_size(inode_t *ip);

int inode_exit(void);

#endif
