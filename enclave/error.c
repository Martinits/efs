#include "error.h"
#include "types.h"
#include "log_types.h"
#include "enclave_t.h"
#include <string.h>

int elog(int type, char *msg, uint32_t len)
{
    int retval;
    return (SGX_SUCCESS == ocall_log(&retval, type, msg, len) && retval == 0) ? 0 : 1;
}

void panic(char *msg)
{
    int retval;
    ocall_log(&retval, LOG_FATAL, msg, (uint32_t)strlen(msg));
    ocall_panic(1);
}
