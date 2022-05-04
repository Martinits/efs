#include "types.h"
#include "bitmap.h"
#include "block.h"
#include "layout.h"
#include "disk.h"
#include "error.h"
#include "superblock.h"
#include "log_types.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

extern superblock_t sb;
uint8_t ibm[BLK_SZ * INODE_BITMAP_CNT];
pthread_mutex_t ibm_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t dbm_first_empty_word = 0; // with probability

int bitmap_init(void)
{
    for(uint32_t i = 0; i < INODE_BITMAP_CNT; i++){
        uint32_t bid = i + INODE_BITMAP_START;
        uint8_t *p = ibm + i * BLK_SZ;

        if(0 != disk_read(p, bid)) return 1;

        if(0 != aes128_block_decrypt(&sb.aes_iv[SB_KEY_IDX(bid)],
                                        &sb.aes_key[SB_KEY_IDX(bid)], p))
            return 1;

        if(0 != sha256_validate(p, &sb.hash[SB_KEY_IDX(bid)])){
            char buf[51] = {0};
            snprintf(buf, 50, "block hash validation failed, bid = %d", bid);
            panic(buf);
            return 1;
        }
    }

    return 0;
}

uint16_t ibm_alloc(void)
{
    pthread_mutex_lock(&ibm_lock);

    uint32_t ret = 0;
    uint64_t *p = (uint64_t *)(ibm + sizeof(ibm)) - 1;
    int tmp;

    for(;(uint8_t *)p >= ibm; p--, ret += sizeof(*p) * 8){
        tmp = (int)BITMAP_FIRST_EMPTY_64(*p);
        if(tmp != -1) break;
    }

    if(tmp == -1) return 0;

    *p |= 1 << tmp;
    ret += tmp;

    pthread_mutex_unlock(&ibm_lock);

    return (uint16_t)ret;
}

int ibm_free(uint16_t iid)
{
    pthread_mutex_lock(&ibm_lock);

    uint8_t *p = ibm + sizeof(ibm) - (iid/8 + 1);

    *p &= (uint8_t)~(1U << (iid%8));

    pthread_mutex_unlock(&ibm_lock);

    return 0;
}

uint32_t dbm_alloc(void)
{
    uint32_t bid = BITMAP_WORDID2BID(dbm_first_empty_word);
    uint32_t ret = dbm_first_empty_word * sizeof(uint64_t) * 8;
    uint32_t wid = dbm_first_empty_word % BITMAP_WORD_PER_BLOCK;

    for(; bid >= BITMAP_START; bid--, wid = 0){
        sb_lock();
        block_t *bp = bget_from_cache_lock(bid, NULL, &sb.aes_iv[SB_KEY_IDX(bid)],
                                            &sb.aes_key[SB_KEY_IDX(bid)],
                                            &sb.hash[SB_KEY_IDX(bid)]);
        sb_unlock();
        if(bp == NULL){
            panic("bitmap: cannot get bitmap block");
            return 0;
        }

        uint64_t *p = (uint64_t *)(bp->data + BLK_SZ);
        p -= wid + 1;

        int tmp;
        for(;(uint8_t *)p >= bp->data; p--, ret += sizeof(*p) * 8){
            tmp = (int)BITMAP_FIRST_EMPTY_64(*p);
            if(tmp != -1) break;
        }

        if(tmp != -1){
            *p |= 1 << tmp;
            ret += tmp;

            // if this word is full, move to next word
            if(~(*p) == 0) dbm_first_empty_word++;

            block_make_dirty(bp->bid);
            block_unlock_return(bp);

            if(sb.nblock < DID2BID(ret)) sb.nblock = DID2BID(ret);

            return ret;
        }

        block_unlock_return(bp);
    }

    panic("bitmap: data bitmap id full");
    return 0;
}


int dbm_free(uint32_t did)
{
    if(did / (BLK_SZ * 8) >= BITMAP_CNT) return 1;

    uint32_t bid = BITMAP_DID2BID(did);

    sb_lock();
    block_t *bp = bget_from_cache_lock(bid, NULL, &sb.aes_iv[SB_KEY_IDX(bid)],
                                        &sb.aes_key[SB_KEY_IDX(bid)],
                                        &sb.hash[SB_KEY_IDX(bid)]);
    sb_unlock();
    if(bp == NULL){
        panic("bitmap: cannot get bitmap block");
        return 0;
    }

    uint32_t blk_offset = did % (BLK_SZ * 8);
    uint8_t *p = bp->data + BLK_SZ - 1 - blk_offset / 8;

    *p &= (uint8_t)~(1U << (blk_offset%8));

    return block_make_dirty(bp->bid) || block_unlock_return(bp);
}

int bitmap_exit()
{
    // write back ibm
    // no need update hash in sb
    for(uint32_t i = 0; i < INODE_BITMAP_CNT; i++){
        uint32_t bid = i + INODE_BITMAP_START;
        uint8_t *p = ibm + i * BLK_SZ;

        if(0 != aes128_block_encrypt(&sb.aes_iv[SB_KEY_IDX(bid)],
                                        &sb.aes_key[SB_KEY_IDX(bid)], p))
            return 1;

        if(0 != disk_write(p, bid)) continue;
    }

    return 0;
}
