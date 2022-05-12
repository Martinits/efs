#include "error.h"
#include "types.h"
#include "log_types.h"
#include "enclave_t.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int elog(int type, const char *msg, ...)
{
    if(type != LOG_FATAL && type != LOG_INFO) return 0;

    char buf[200] = {0};

    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    int retval;
    return (SGX_SUCCESS == ocall_log(&retval, type, buf, (uint32_t)strlen(buf) + 1) && retval == 0) ? 0 : 1;
}

void panic(const char *msg, ...)
{
    char buf[200] = {0};

    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    int retval;
    ocall_log(&retval, LOG_FATAL, buf, (uint32_t)strlen(buf) + 1);
    ocall_panic(1);
}
