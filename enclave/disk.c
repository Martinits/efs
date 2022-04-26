#include "types.h"
#include "disk.h"
#include "layout.h"

int disk_read(uint8_t *buf, uint32_t bid)
{
    int retval;
    if(SGX_SUCCESS != ocall_disk_read(&retval, buf, bid) || retval != 0)
        return 1;
    return 0;
}

int disk_write(uint8_t *buf, uint32_t bid)
{
    int retval;
    if(SGX_SUCCESS != ocall_disk_write(&retval, buf, bid) || retval != 0)
        return 1;
    return 0;
}

