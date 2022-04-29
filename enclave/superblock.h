#ifndef _SUPERBLOCK_H
#define _SUPERBLOCK_H

#include "types.h"

int sb_init(void);

int sb_lock(void);

int sb_unlock(void);

int sb_exit(void);

#endif

