#include "types.h"
#include "disk.h"
#include "layout.h"
#include <pthread.h>

pthread_mutex_t disk_lock = PTHREAD_MUTEX_INITIALIZER;

int disk_init(int backend_type, uint8_t *zero_encrypted)
{
    int retval;
    if(SGX_SUCCESS != ocall_disk_init(&retval, backend_type, zero_encrypted) || retval != 0)
        return 1;

    return 0;
}

int disk_read(uint8_t *buf, uint32_t bid)
{
    pthread_mutex_lock(&disk_lock);

    int retval;
    if(SGX_SUCCESS != ocall_disk_read(&retval, buf, bid) || retval != 0){
        pthread_mutex_unlock(&disk_lock);
        return 1;
    }

    pthread_mutex_unlock(&disk_lock);
    return 0;
}

int disk_write(uint8_t *buf, uint32_t bid)
{
    pthread_mutex_lock(&disk_lock);

    int retval;
    if(SGX_SUCCESS != ocall_disk_write(&retval, buf, bid) || retval != 0){
        pthread_mutex_unlock(&disk_lock);
        return 1;
    }

    pthread_mutex_unlock(&disk_lock);
    return 0;
}

int disk_setzero(uint32_t bid)
{
    pthread_mutex_lock(&disk_lock);

    int retval;
    if(SGX_SUCCESS != ocall_disk_setzero(&retval, bid) || retval != 0){
        pthread_mutex_unlock(&disk_lock);
        return 1;
    }

    pthread_mutex_unlock(&disk_lock);
    return 0;
}

int disk_exit(void)
{
    int retval;
    if(SGX_SUCCESS != ocall_disk_exit(&retval) || retval != 0)
        return 1;

    return 0;
}
