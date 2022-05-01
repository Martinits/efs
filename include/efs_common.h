#ifndef _EFS_COMMON_H
#define _EFS_COMMON_H

#define BLK_SZ (4096)
#define BLK_SZ_BITS (12)
#define DISK_OFFSET(bid) (bid << BLK_SZ_BITS)

#define BACKEND_TP_FILE (0)
#define BACKEND_TP_DISK (1)

#define BACKEND_FILE_NAME ("efsdata")

#endif
