#ifndef _SECURITY_H
#define _SECURITY_H

#include "types.h"
#include "efs_common.h"

int security_init(const key128_t * iv, const key128_t *key);

int aes128_block_encrypt(const key128_t *iv, const key128_t *key, uint8_t *data);

int aes128_block_decrypt(const key128_t *iv, const key128_t *key, uint8_t *data);

int sha256_block(const uint8_t *data, key256_t *hash);

int sha256_validate(const uint8_t *data, const key256_t *exp_hash);

int sha256_sb(const uint8_t *data, key256_t *hash);

int sha256_sb_validate(const uint8_t *data, const key256_t *exp_hash);

key256_t *zero_block_sha256(void);

int key128_gen(key128_t *key);

int security_exit(void);

#endif
