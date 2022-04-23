#include "types.h"
#include "security.h"
#include "sgx_trts.h"
#include "layout.h"
#include "enclave_t.h"
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>

int aes128_block_encrypt(const key128_t *iv, const key128_t *key, uint8_t *data)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if(!ctx) return 1;

    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key->k, iv->k))
        return 1;

    if(1 != EVP_CIPHER_CTX_set_padding(ctx, 0))
        return 1;

    int outlen = 0, tmplen;
    uchar outbuf[BLK_SZ * 2] = {0};

    if(1 != EVP_EncryptUpdate(ctx, outbuf, &tmplen, data, BLK_SZ))
        return 1;
    outlen += tmplen;

    if(1 != EVP_EncryptFinal_ex(ctx, outbuf + outlen, &tmplen))
        return 1;
    outlen += tmplen;

    EVP_CIPHER_CTX_free(ctx);

    if(outlen != BLK_SZ) return 1;

    memcpy(data, outbuf, BLK_SZ);

    return 0;
}

int aes128_block_decrypt(const key128_t *iv, const key128_t *key, uint8_t *data)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if(!ctx) return 1;

    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key->k, iv->k))
        return 1;

    if(1 != EVP_CIPHER_CTX_set_padding(ctx, 0))
        return 1;

    int outlen = 0, tmplen;
    uchar outbuf[BLK_SZ+1] = {0};

    if(1 != EVP_DecryptUpdate(ctx, outbuf, &tmplen, data, BLK_SZ))
        return 1;
    outlen += tmplen;

    if(1 != EVP_DecryptFinal_ex(ctx, outbuf + outlen, &tmplen))
        return 1;
    outlen += tmplen;

    EVP_CIPHER_CTX_free(ctx);

    if(outlen != BLK_SZ) return 1;

    memcpy(data, outbuf, BLK_SZ);

    return 0;
}

int sha256_block(const uint8_t *data, key256_t *hash)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    if(!ctx) return 1;

    if(1 != EVP_DigestInit_ex(ctx, EVP_sha256(), NULL))
        return 1;

    if(1 != EVP_DigestUpdate(ctx, data, BLK_SZ))
        return 1;

    uint len;
    uint8_t *digest = (uint8_t *)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    if(!digest) return 1;

    if(1 != EVP_DigestFinal_ex(ctx, digest, &len))
        return 1;

    EVP_MD_CTX_free(ctx);

    if(len != 32) return 1;

    memcpy(hash->k, digest, sizeof(hash->k));

    OPENSSL_free(digest);

    return 0;
}

int sha256_validate(const uint8_t *data, const key256_t *exp_hash)
{
    key256_t hash;
    if(0 != sha256_block(data, &hash)) return 1;

    for(ulong i = 0; i < sizeof(hash.k)/sizeof(hash.k[0]); i++){
        if(hash.k[i] ^ exp_hash->k[i]) return 1;
    }

    return 0;
}
