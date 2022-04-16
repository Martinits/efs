#ifndef _DISK_H
#define _DISK_H

#include "types.h"
#include "sgx_trts.h"
#include "enclave_t.h"

int disk_read(uint8_t *buf, uint32_t bid);

int disk_write(uint8_t *buf, uint32_t bid);

#endif
