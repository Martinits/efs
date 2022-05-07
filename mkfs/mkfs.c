#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "layout.h"
#include "efs_common.h"
#include "inode.h"
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <time.h>
#include "test_common.h"

key256_t zero_sha256, sb_hash;
key128_t iv, key;

#undef SB_IV_PTR
#undef SB_KEY_PTR
#undef SB_HASH_PTR
#define SB_KEY_PTR(bid) (&sb->aes_key[SB_KEY_IDX(bid)])
#define SB_IV_PTR(bid) (&sb->aes_iv[SB_KEY_IDX(bid)])
#define SB_HASH_PTR(bid) (&sb->hash[SB_KEY_IDX(bid)])

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

static int sha256_calc(const uint8_t *data, uint32_t size, key256_t *hash)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    if(!ctx) return 1;

    if(1 != EVP_DigestInit_ex(ctx, EVP_sha256(), NULL))
        return 1;

    if(1 != EVP_DigestUpdate(ctx, data, size))
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

int sha256_block(const uint8_t *data, key256_t *hash)
{
    return sha256_calc(data, BLK_SZ, hash);
}

int sha256_sb(const uint8_t *data, key256_t *hash)
{
    return sha256_calc(data, BLK_SZ * SUPERBLOCK_CNT, hash);
}

int key128_gen(key128_t *key)
{
    for(int i = 0; i < sizeof(key->k)/sizeof(key->k[0]); i++)
        key->k[i] = rand() % 256;
    return 0;
}

int main()
{
    srand(time(NULL));

    if(0 != key128_gen(&iv)) return 1;
    if(0 != key128_gen(&key)) return 1;
    FILE *fp = fopen(EFS_IKH, "w");
    if(!fp) return 1;
    if(1 != fwrite(&iv, sizeof(key128_t), 1, fp)) return 1;
    if(1 != fwrite(&key, sizeof(key128_t), 1, fp)) return 1;
    if(0 != fclose(fp)) return 1;

    // truncate if exists, create if not
    fp = fopen(EFS_DISK_NAME, "w");
    if(!fp) return 1;
    if(0 != fclose(fp)) return 1;

    fp = fopen(EFS_DISK_NAME, "r+");
    if(!fp) return 1;

    uint8_t data[BLK_SZ] = {0}, sbdata[BLK_SZ * SUPERBLOCK_CNT] = {0};

    //calc zero hash
    if(0 != sha256_block(data, &zero_sha256)) return 1;

    //superblock
    superblock_t *sb = (superblock_t *)sbdata;
    sb->magic = EFS_MAGIC;
    sb->nblock = DATA_START;
    sb->rootinode = 0;
    for(uint i = INODE_BITMAP_START; i < DATA_START; i++){
        if(0 != key128_gen(SB_IV_PTR(i))) return 1;
        if(0 != key128_gen(SB_KEY_PTR(i))) return 1;
    }

    if(0 != fseek(fp, BLK_SZ * INODE_BITMAP_START, SEEK_SET)) return 1;

    //inode bitmap
    for(uint32_t i = INODE_BITMAP_START; i < INODE_START; i++){
        memset(data, 0, BLK_SZ);
        if(i == INODE_BITMAP_START){
            // only one inode -- rootinode
            uint64_t *tmp = (uint64_t *)data;
            *tmp = 1UL << 63;
            if(0 != sha256_block(data, SB_HASH_PTR(i))) return 1;
        }else{
            memcpy(SB_HASH_PTR(i), &zero_sha256, sizeof(key256_t));
        }
        if(0 != aes128_block_encrypt(SB_IV_PTR(i), SB_KEY_PTR(i), data)) return 1;
        if(1 != fwrite(data, BLK_SZ, 1, fp)) return 1;
    }

    //inode
    for(uint32_t i = INODE_START; i < BITMAP_START; i++){
        memset(data, 0, BLK_SZ);
        if(i == INODE_START){
            dinode_t *dip = (dinode_t *)data;
            dip->type = INODE_TP_DIR;
            dip->size = 0;
            if(0 != key128_gen(&dip->aes_iv)) return 1;
            if(0 != key128_gen(&dip->aes_key)) return 1;
            if(0 != sha256_block(data, SB_HASH_PTR(i))) return 1;
        }else{
            memcpy(SB_HASH_PTR(i), &zero_sha256, sizeof(key256_t));
        }
        if(0 != aes128_block_encrypt(SB_IV_PTR(i), SB_KEY_PTR(i), data)) return 1;
        if(1 != fwrite(data, BLK_SZ, 1, fp)) return 1;
    }

    //bitmap
    for(uint32_t i = BITMAP_START; i < DATA_START; i++){
        memset(data, 0, BLK_SZ);
        memcpy(SB_HASH_PTR(i), &zero_sha256, sizeof(key256_t));
        if(0 != aes128_block_encrypt(SB_IV_PTR(i), SB_KEY_PTR(i), data)) return 1;
        if(1 != fwrite(data, BLK_SZ, 1, fp)) return 1;
    }

    //superblock hash
    if(0 != sha256_sb(sbdata, &sb_hash)) return 1;
    for(uint32_t i = 0; i < SUPERBLOCK_CNT; i++){
        if(0 != aes128_block_encrypt(&iv, &key, sbdata + i * BLK_SZ)) return 1;
    }
    if(0 != fseek(fp, 0, SEEK_SET)) return 1;
    if(1 != fwrite(sbdata, sizeof(sbdata), 1, fp)) return 1;
    if(0 != fclose(fp)) return 1;

    //write sb_hash
    fp = fopen(EFS_IKH, "a");
    if(!fp) return 1;
    if(1 != fwrite(&sb_hash, sizeof(key256_t), 1, fp)) return 1;
    if(0 != fclose(fp)) return 1;

    printf("\niv: \n");
    for(int i=0;i < 16; i++) printf("%02x ", iv.k[i]);

    printf("\nkey: \n");
    for(int i=0;i < 16; i++) printf("%02x ", key.k[i]);

    printf("\nhash: \n");
    for(int i=0;i<32;i++) printf("%02x ", sb_hash.k[i]);
    puts("");
    return 0;
}
