#include "types.h"
#include "cache.h"
#include "Enclave_t.h"
#include <string.h>

int ecall_efs_init(void)
{
    int retval;
    uint8_t buf[100]={0};

    if(ocall_disk_read(&retval, buf, 0, sizeof(buf)) != SGX_SUCCESS)
        return 1;
    if(strcmp(buf, "bufread4096"))
        return 2;

    if(ocall_disk_write(&retval, buf, 0, sizeof(buf)) != SGX_SUCCESS)
        return 3;

    return cache_init();
}
