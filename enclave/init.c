#include "types.h"
#include "cache.h"
#include "enclave_t.h"
#include <string.h>

int ecall_efs_init(void)
{
    return cache_init();
}
