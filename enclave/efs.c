#include "types.h"
#include "file.h"
#include "disk.h"
#include "security.h"
#include "superblock.h"
#include "bitmap.h"
#include "block.h"
#include "inode.h"

extern uint8_t zero_encrypted[BLK_SZ];

int efs_init(const key128_t *iv, const key128_t *key, const key256_t *exp_hash, int backend_type)
{
    if(0 != security_init(iv, key)) return 1;

    if(0 != disk_init(backend_type, zero_encrypted)) return 1;

    if(0 != sb_init(iv, key, exp_hash)) return 1;

    if(0 != bitmap_init()) return 1;

    if(0 != block_init()) return 1;

    if(0 != inode_init()) return 1;

    if(0 != file_init()) return 1;

    return 0;
}

int efs_exit(const key128_t *iv, const key128_t *key, key256_t *hash)
{
    if(0 != file_exit()) return 1;

    if(0 != inode_exit()) return 1;

    if(0 != block_exit()) return 1;

    if(0 != bitmap_exit()) return 1;

    if(0 != sb_exit(iv, key, hash)) return 1;

    if(0 != security_exit()) return 1;

    if(0 != disk_exit()) return 1;

    return 0;
}
