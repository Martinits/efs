#include "types.h"
#include "enclave_u.h"
#include <stdio.h>
#include <string.h>
#include "sgx_urts.h"
#include "entry.h"
#include <pthread.h>
#include "log.h"
#include "log_types.h"

pthread_spinlock_t log_lock;

static void log_lock_unlock(bool lock, void *udata)
{
    (void)(udata);

    if(lock)
        pthread_spin_lock(&log_lock);
    else
        pthread_spin_unlock(&log_lock);
}

int ocall_init(void)
{
    if(0 != pthread_spin_init(&log_lock, PTHREAD_PROCESS_PRIVATE))
        return 1;
    log_set_lock(log_lock_unlock, NULL);
    return 0;
}

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

int ocall_log(int log_type, char *msg, uint32_t len)
{
    if(strlen(msg) != len)
        return 1;

    switch(log_type)
    {
        case LOG_TRACE : log_trace(msg); break;
        case LOG_DEBUG : log_debug(msg); break;
        case LOG_INFO  : log_info(msg);  break;
        case LOG_WARN  : log_warn(msg);  break;
        case LOG_ERROR : log_error(msg); break;
        case LOG_FATAL : log_fatal(msg); break;
    }
    return 0;
}

void ocall_panic(int exitcode)
{
    log_fatal("Enclave panic and exit with code %d\n", exitcode);

    while(1);

    if(SGX_SUCCESS == sgx_destroy_enclave(global_eid)){
        fprintf(stdout, "Enclave %ld destroyed\n", global_eid);
    }else{
        fprintf(stdout, "Cannot destroy enclave %ld\n", global_eid);
    }
}
