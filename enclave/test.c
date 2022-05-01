#include "efs.h"
#include "sgx_trts.h"
#include "enclave_t.h"

int ecall_efs_test(void)
{
    return efs_init(NULL, NULL, NULL, BACKEND_TP_FILE);
}
