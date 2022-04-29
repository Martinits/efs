#include "types.h"
#include "superblock.h"
#include "layout.h"
#include <pthread.h>

superblock_t sb;
pthread_mutex_t sblock = PTHREAD_MUTEX_INITIALIZER;

int sb_init(void)
{

}

int sb_lock(void)
{
    return pthread_mutex_lock(&sblock);
}

int sb_unlock(void)
{
    return pthread_mutex_unlock(&sblock);
}

int sb_exit(void)
{

}
