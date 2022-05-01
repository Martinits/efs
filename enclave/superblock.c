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

    //check metadata
    for(uint32_t bid = INODE_BITMAP_START; bid < DATA_START; bid++){
        if(0 != disk_read(data, bid))
            return 1;

        if(0 != aes128_block_decrypt(iv, key, data))
            return 1;

        if(0 != sha256_sb_validate(data, exp_hash)){
            char buf[51] = {0};
            snprintf(buf, 50, "block hash validation failed during init, bid = %d", bid);
            panic(buf);
            return 1;
        }
    }

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

int sb_exit(void)
{

}
