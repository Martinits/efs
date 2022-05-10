#include "types.h"
#include "superblock.h"
#include "layout.h"
#include "security.h"
#include "disk.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

superblock_t sb;
pthread_mutex_t sblock = PTHREAD_MUTEX_INITIALIZER;

int sb_init(const key128_t *iv, const key128_t *key, const key256_t *exp_hash)
{
    elog(LOG_INFO, "sb init");

    // load sb
    uint8_t data[BLK_SZ * SUPERBLOCK_CNT] = {0};

    for(uint i = 0; i < SUPERBLOCK_CNT; i++){
        if(0 != disk_read(data + i * BLK_SZ, i))
            return 1;

        if(0 != aes128_block_decrypt(iv, key, data + i * BLK_SZ))
            return 1;
    }

    if(0 != sha256_sb_validate(data, exp_hash)){
        panic("superblock hash validate failed");
        return 1;
    }

    memcpy(&sb, data, sizeof(sb));
    memset(data, 0, sizeof(data));

    //check metadata
    for(uint32_t bid = INODE_BITMAP_START; bid < DATA_START; bid++){
        if(0 != disk_read(data, bid))
            return 1;

        if(0 != aes128_block_decrypt(SB_IV_PTR(bid), SB_KEY_PTR(bid), data))
            return 1;

        if(0 != sha256_validate(data, SB_HASH_PTR(bid))){
            panic("block hash validation failed during init, bid = %d", bid);
            return 1;
        }
    }

    elog(LOG_INFO, "sb init done");

    return 0;
}

int sb_lock(void)
{
    return pthread_mutex_lock(&sblock);
}

int sb_unlock(void)
{
    return pthread_mutex_unlock(&sblock);
}

int sb_exit(const key128_t *iv, const key128_t *key, key256_t *hash)
{
    elog(LOG_INFO, "sb exit");

    sb.magic = EFS_MAGIC;
    sb.rootinode = 0;

    uint8_t data[BLK_SZ * SUPERBLOCK_CNT] = {0};

    // must update all metadata block hash
    for(uint32_t bid = INODE_BITMAP_START; bid < DATA_START; bid++){
        if(0 != disk_read(data, bid))
            return 1;

        if(0 != aes128_block_decrypt(SB_IV_PTR(bid), SB_KEY_PTR(bid), data))
            return 1;

        // change iv and key for metadata
        if(0 != key128_gen(SB_IV_PTR(bid))) return 1;
        if(0 != key128_gen(SB_KEY_PTR(bid))) return 1;

        if(0 != sha256_block(data, SB_HASH_PTR(bid)))
            return 1;

        if(0 != aes128_block_encrypt(SB_IV_PTR(bid), SB_KEY_PTR(bid), data))
            return 1;

        if(0 != disk_write(data, bid))
            return 1;
    }

    memset(data, 0, sizeof(data));
    memcpy(data, &sb, sizeof(superblock_t));

    if(0 != sha256_sb(data, hash))
        return 1;

    for(uint i = 0; i < SUPERBLOCK_CNT; i++){
        if(0 != aes128_block_encrypt(iv, key, data + i * BLK_SZ))
            return 1;

        if(0 != disk_write(data + i * BLK_SZ, i))
            return 1;
    }

    elog(LOG_INFO, "sb exit done");

    return 0;
}
