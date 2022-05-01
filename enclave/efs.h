#ifndef _EFS_H
#define _EFS_H

#include "types.h"
#include "security.h"
#include "efs_common.h"

#define O_READONLY (0x00)
#define O_WRITE    (0x01)
#define O_CREATE   (0x02)
#define O_APPEND   (0x04)

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

typedef struct _file file;

int efs_init(const key128_t *iv, const key128_t *key, const key256_t *exp_hash, int backend_type);

int efs_exit(const key128_t *iv, const key128_t *key, key256_t *hash);

file *fopen(const char *filename, int flags);

int fclose(file *fp);

uint32_t fread(file *fp, void *data, uint32_t size);

uint32_t fwrite(file *fp, void *data, uint32_t size);

int feof(file *fp);

uint32_t ftell(file *fp);

int fseek(file *fp, uint32_t off, int whence);

int mkdir(const char *path);

int rmdir(const char *path);

int rmfile(const char *path);

#endif
