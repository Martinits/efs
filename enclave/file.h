#ifndef _FILE_H
#define _FILE_H

#include "types.h"
#include "inode.h"
#include <pthread.h>

#define O_READONLY (0x00)
#define O_WRITE    (0x01)
#define O_CREATE   (0x02)
#define O_APPEND   (0x04)

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

typedef struct _file {
    int writable;
    uint32_t offset;
    inode_t *ip;
    pthread_mutex_t lock;
    char path[0];
} file;

int file_init(void);

int file_exit(void);

#endif
