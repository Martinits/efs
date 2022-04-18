#ifndef _SECURITY_H
#define _SECURITY_H

#include "types.h"

struct key128 {
    uchar k[16];
};

struct key256 {
    uchar k[32];
};

int aes128_block_encrypt(const struct key128 *iv, const struct key128 *key, uint8_t *data);

int aes128_block_decrypt(const struct key128 *iv, const struct key128 *key, uint8_t *data);

int sha256_block(uint8_t *data, struct key256 *hash);

#endif
