#ifndef _BITMAP_H
#define _BITMAP_H

#include "types.h"
#include "layout.h"

int bitmap_init(void);

uint16_t ibm_find_empty_and_set(void);

int ibm_free(uint16_t iid);

uint32_t dbm_find_empty_and_set(void);

int dbm_free(uint32_t did);

#endif
