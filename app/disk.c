#include "types.h"
#include "enclave_u.h"
#include <stdio.h>
#include <string.h>

int ocall_disk_read(uint8_t *buf, uint32_t offset, uint32_t len)
{
    printf("bufread4096\n");
    return 0;
}

int ocall_disk_write(uint8_t *buf, uint32_t offset, uint32_t len)
{
    printf("bufwrite4096\n");
    return 0;
}

