#ifndef _DISK_LAYOUT_H
#define _DISK_LAYOUT_H

#define BLK_SZ (4096)
#define BLK_SZ_BITS (12)
#define DISK_OFFSET(bid) (bid << BLK_SZ_BITS)

#endif
