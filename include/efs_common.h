#ifndef _EFS_COMMON_H
#define _EFS_COMMON_H

#define BLK_SZ (4096)
#define BLK_SZ_BITS (12)
#define DISK_OFFSET(bid) (bid << BLK_SZ_BITS)

#define BACKEND_TP_FILE (0)
#define BACKEND_TP_DISK (1)

#define EFS_DISK_NAME ("efsdisk")

#include "types.h"

typedef struct {
    uchar k[16];
} key128_t;

typedef struct {
    uchar k[32];
} key256_t;

#endif
