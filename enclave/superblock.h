#ifndef _SUPERBLOCK_H
#define _SUPERBLOCK_H

#include "types.h"
#include "security.h"

int sb_init(const key128_t *iv, const key128_t *key, const key256_t *exp_hash);

int sb_lock(void);

int sb_unlock(void);

int sb_exit(const key128_t *iv, const key128_t *key, key256_t *hash);

#endif

