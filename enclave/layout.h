#ifndef _LAYOUT_H
#define _LAYOUT_H

#include "types.h"
#include "security.h"

#define NDIRECT (10)

struct dinode {
    uint32_t type;
    uint32_t size;
    uint32_t did[NDIRECT + 3];
    struct key128 aes_key;
    struct key256 hash[NDIRECT + 3];
};

struct index {
    uint32_t did;
    struct key256 hash;
};

struct dirent {
    uint16_t iid;
    char name[14];
};

#define BITCOUNT32(n) ({                                      \
                        uint32_t u = (n);                     \
                        u -= ((n) >> 1) & 03333333333;        \
                        u -= ((n) >> 2) & 011111111111;       \
                        ((u + (u >> 3)) & 030707070707) % 63; \
                    })

#define NEXT_POW2_INCLUDE_32(n) ({                    \
                            uint32_t i = (n); \
                            --i;              \
                            i |= i >> 1;      \
                            i |= i >> 2;      \
                            i |= i >> 4;      \
                            i |= i >> 8;      \
                            i |= i >> 16;     \
                            i + 1;            \
                        })

#define BITCOUNT64(n) ({                                             \
                        uint64_t u = (n);                            \
                        u -= ((n) >> 1) & 0x7777777777777777;        \
                        u -= ((n) >> 2) & 0x3333333333333333;        \
                        u -= ((n) >> 3) & 0x1111111111111111;        \
                        ((u + (u >> 4)) & 0x0f0f0f0f0f0f0f0f) % 255; \
                    })

#define NEXT_POW2_64(n) ({                    \
                            uint64_t i = (n); \
                            i |= i >> 1;      \
                            i |= i >> 2;      \
                            i |= i >> 4;      \
                            i |= i >> 8;      \
                            i |= i >> 16;     \
                            i |= i >> 32;     \
                            i + 1;            \
                        })

#define LEFTMOST_SET_BIT_MASK_64(n) (NEXT_POW2_64(n) >> 1)
#define LEFTMOST_SET_BIT_64(n) ((n) == 0 ? -1 : BITCOUNT64(LEFTMOST_SET_BIT_MASK_64(n) - 1))

#define BLK_SZ (4096)
#define BLK_SZ_BITS (12)
#define DISK_OFFSET(bid) (bid << BLK_SZ_BITS)

#define SUPERBLOCK_START (0)
#define SUPERBLOCK_CNT ((sizeof(struct superblock) >> BLK_SZ_BITS) + 1)

#define INODE_BITMAP_START (SUPERBLOCK_START + SUPERBLOCK_CNT)
#define INODE_BITMAP_CNT (1)
#define BITMAP_FIRST_EMPTY_64(n) (63 - LEFTMOST_SET_BIT_64(n))

#define INODE_START (INODE_BITMAP_START + INODE_BITMAP_CNT)
#define INODE_SZ NEXT_POW2_INCLUDE_32(sizeof(struct dinode))
#define INODE_PER_BLOCK (BLK_SZ / INODE_SZ)
#define INODE_CNT (8 * INODE_BITMAP_CNT * 512 /*INODE_SZ*/)
#define INODE_DIKS_OFFSET(iid) (INODE_START + iid / INODE_PER_BLOCK)
#define INODE_BLOCK_OFFSET(iid) ((iid % INODE_PER_BLOCK) * INODE_SZ)

#define BITMAP_START (INODE_START + INODE_CNT)
#define BITMAP_CNT (512) // 64G

#define DATA_START (BITMAP_START + BITMAP_CNT)

struct superblock {
    uint32_t magic;
    uint32_t nblock;
    uint32_t ibitmap_start;
    uint32_t nibitmap;
    uint32_t inode_start;
    uint32_t ninode;
    uint32_t dbitmap_start;
    uint32_t ndbitmap;
    uint32_t data_start;
    uint32_t ndata;
    struct key128 aes_key[INODE_CNT];
    struct key256 hash[INODE_CNT];
};

#define EFS_MAGIC 0x04546530

#endif
