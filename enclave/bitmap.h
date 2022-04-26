#ifndef _BITMAP_H
#define _BITMAP_H

#include "types.h"
#include "layout.h"

#define BITCOUNT64(n) ({                                             \
                        uint64_t u = (n);                            \
                        u -= ((n) >> 1) & 0x7777777777777777;        \
                        u -= ((n) >> 2) & 0x3333333333333333;        \
                        u -= ((n) >> 3) & 0x1111111111111111;        \
                        ((u + (u >> 4)) & 0x0f0f0f0f0f0f0f0f) % 255; \
                    })

#define NEXT_POW2_64(n) ({                    \
                            uint64_t i = (n); \
                            i |= i >> 1;      \
                            i |= i >> 2;      \
                            i |= i >> 4;      \
                            i |= i >> 8;      \
                            i |= i >> 16;     \
                            i |= i >> 32;     \
                            i + 1;            \
                        })

#define LEFTMOST_SET_BIT_MASK_64(n) (NEXT_POW2_64(n) >> 1)
#define LEFTMOST_SET_BIT_64(n) BITCOUNT64(LEFTMOST_SET_BIT_MASK_64(n) - 1)
#define BITMAP_FIRST_EMPTY_64(n) ((~(n) == 0) ? -1 : (63 - LEFTMOST_SET_BIT_64(~(n))))

#define BITMAP_WORD_PER_BLOCK (BLK_SZ / sizeof(uint64_t))
#define BITMAP_WORDID2BID(wid) (BITMAP_START + BITMAP_CNT - 1 - (wid)/BITMAP_WORD_PER_BLOCK)
#define BITMAP_DID2BID(did) (BITMAP_START + BITMAP_CNT - 1 - (did)/(BLK_SZ * 8))

int bitmap_init(void);

uint16_t ibm_alloc(void);

int ibm_free(uint16_t iid);

uint32_t dbm_alloc(void);

int dbm_free(uint32_t did);

int bitmap_exit();

#endif
