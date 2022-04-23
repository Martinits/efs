#include "types.h"
#include "cache.h"
#include "sgx_trts.h"
#include "enclave_t.h"
#include <string.h>
#include "layout.h"

superblock_t sb;

int ecall_efs_init(void)
{
    // load sb
    // decrypt and check intergrity
    return 0;
}
