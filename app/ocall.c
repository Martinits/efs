#include "types.h"
#include "enclave_u.h"
#include <stdio.h>
#include <string.h>
#include "sgx_urts.h"
#include "entry.h"
#include <pthread.h>
#include "log.h"
#include "log_types.h"
#include "efs_common.h"

#define BACKEND_FILE_NAME EFS_DISK_NAME

pthread_spinlock_t log_lock;

uint8_t blk_zero_encrypted[BLK_SZ] = {0};

FILE *backend_fp = NULL;

static void log_lock_unlock(bool lock, void *udata)
{
    (void)(udata);

    if(lock)
        pthread_spin_lock(&log_lock);
    else
        pthread_spin_unlock(&log_lock);
}

int ocall_disk_init(int backend_type, uint8_t *zero_encrypted)
{
    if(0 != pthread_spin_init(&log_lock, PTHREAD_PROCESS_PRIVATE))
        return 1;
    log_set_lock(log_lock_unlock, NULL);

    // prepare back end
    switch(backend_type){
        case BACKEND_TP_FILE: {
            backend_fp = fopen(BACKEND_FILE_NAME, "r+");
            if(!backend_fp) return 1;
            break;
        }
        case BACKEND_TP_DISK: {
            //todo
            break;
        }
        default: return 1;
    }

    // prepare blk_zero_encrypted
    memcpy(blk_zero_encrypted, zero_encrypted, BLK_SZ);

    return 0;
}

int ocall_disk_read(uint8_t *buf, uint32_t bid)
{
    // if not exist, augment disk to desired size
    if(0 != fseek(backend_fp, bid * BLK_SZ, SEEK_SET)) return 1;

    if(1 == fread(buf, BLK_SZ, 1, backend_fp)) return 0;

    if(1 != fwrite(blk_zero_encrypted, BLK_SZ, 1, backend_fp)) return 1;

    memcpy(buf, blk_zero_encrypted, BLK_SZ);
    return 0;
}

int ocall_disk_write(uint8_t *buf, uint32_t bid)
{
    if(0 != fseek(backend_fp, bid * BLK_SZ, SEEK_SET)) return 1;

    return 1 != fwrite(buf, BLK_SZ, 1, backend_fp);
}

int ocall_disk_setzero(uint32_t bid)
{
    if(0 != fseek(backend_fp, bid * BLK_SZ, SEEK_SET)) return 1;

    return 1 != fwrite(blk_zero_encrypted, BLK_SZ, 1, backend_fp);
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

int ocall_disk_exit(void)
{
    fclose(backend_fp);

    return 0;
}
