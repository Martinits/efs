#include "file.h"
#include "enclave_t.h"
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "set.h"
#include "layout.h"
#include <pthread.h>
#include <ctype.h>

struct fset {
    file *fp;
    struct fset *next;
} fset_head;

set_t pathset;
pthread_mutex_t fset_lk = PTHREAD_MUTEX_INITIALIZER;

int file_init(void)
{
    fset_head.next = NULL;
    return SET_TRUE != set_init(&pathset);
}

static int fset_lock(void)
{
    return pthread_mutex_lock(&fset_lk);
}

static int fset_unlock(void)
{
    return pthread_mutex_unlock(&fset_lk);
}

static int fset_insert(file *fp)
{
    struct fset *node = (struct fset *)malloc(sizeof(struct fset));
    node->fp = fp;
    node->next = fset_head.next;
    fset_head.next = node;

    return SET_TRUE != set_add(&pathset, fp->path);
}

static int fset_delete(file *fp)
{
    struct fset *p = &fset_head;
    while(p->next && p->next->fp != fp) p = p->next;
    if(p->next){
        struct fset *tmp = p->next;
        p->next = tmp->next;
        free(tmp);
    }

    return SET_TRUE != set_remove(&pathset, fp->path);
}

static int fset_contains(const char *path)
{
    return SET_TRUE == set_contains(&pathset, path);
}

static int file_lock(file *fp)
{
    return pthread_mutex_lock(&fp->lock);
}

static int file_unlock(file *fp)
{
    return pthread_mutex_unlock(&fp->lock);
}

static int path_check(const char *path)
{
    const char *p = path, *tmp;

    while(*p){
        if(*p != '/') return 1;
        p++;

        tmp = p;
        while(*p && isalnum(*p)) p++;
        if(*p){
            if(*p != '/') return 1;
            if(p - tmp > DIRNAME_MAX_LEN || p == tmp) return 1;
        }else{
            if(p - tmp > DIRNAME_MAX_LEN || p == tmp) return 1;
            else break;
        }
    }

    return 0;
}

static int path_split(const char *path, char *upper, char *last)
{
    const char *p = path + strlen(path) - 1, *end = p;

    while(*p != '/') p--;

    memcpy(upper, path, p-path);
    *(upper + (p-path)) = 0;

    memcpy(last, p+1, end-p);
    *(last + (end-p)) = 0;

    return 0;
}

file *fopen(const char *filename, int flags)
{
    if(0 != path_check(filename))
        return NULL;

    fset_lock();

    if(fset_contains(filename))
        goto error;

    uint64_t pathlen = strlen(filename);

    file *fp = (file *)malloc(sizeof(file) + pathlen * (1 + sizeof(char)));
    if(fp == NULL) goto error;

    strncpy(fp->path, filename, pathlen);
    fp->path[pathlen] = 0;

    fp->lock = (typeof(fp->lock))PTHREAD_MUTEX_INITIALIZER;

    //parsing flags
    if(flags & O_RDWR){
        fp->writable = 1;

        fp->ip = inode_get_file(filename, flags & O_CREATE);
        if(fp->ip == NULL) goto error;

        fp->offset = flags & O_APPEND ? inode_get_size(fp->ip) : 0;
    }else{
        fp->writable = 0;

        fp->ip = inode_get_file(filename, 0);
        if(fp->ip == NULL) goto error;

        fp->offset = 0;
    }

    if(0 != fset_insert(fp)) goto error;
    fset_unlock();
    return fp;

error:
    fset_unlock();
    return NULL;
}

int fclose(file *fp)
{
    fset_lock();

    if(!fset_contains(fp->path)){
        fset_unlock();
        return 1;
    }

    fset_delete(fp);
    fset_unlock();

    file_lock(fp);

    //free fp
    if(0 != inode_return(fp->ip)){
        file_unlock(fp);
        return 1;
    }

    free(fp);
    //no file_unlock, since fp is freed

    return 0;
}

uint32_t fread(file *fp, void *data, uint32_t size)
{
    file_lock(fp);

    uint32_t ret = inode_read_file(fp->ip, data, fp->offset, size);

    file_unlock(fp);

    return ret;
}

uint32_t fwrite(file *fp, void *data, uint32_t size)
{
    file_lock(fp);

    uint32_t ret = inode_write_file(fp->ip, data, fp->offset, size);

    file_unlock(fp);

    return ret;
}

int feof(file *fp)
{
    file_lock(fp);

    int ret = inode_get_size(fp->ip) == fp->offset;

    file_unlock(fp);

    return ret;
}

uint32_t ftell(file *fp)
{
    file_lock(fp);

    uint32_t ret = fp->offset;

    file_unlock(fp);

    return ret;
}

int fseek(file *fp, uint32_t off, int whence)
{
    file_lock(fp);

    uint32_t newoff = 0;
    switch(whence){
        case SEEK_SET: newoff = 0; break;
        case SEEK_CUR: newoff = fp->offset; break;
        case SEEK_END: newoff = inode_get_size(fp->ip); break;
        default: file_unlock(fp); return 1;
    }

    fp->offset = newoff + off;

    file_unlock(fp);

    return 0;
}

int mkdir(const char *path)
{
    if(0 != path_check(path))
        return 1;

    uint64_t pathlen = strlen(path);
    char upper[pathlen + 1], last[DIRNAME_MAX_LEN + 1];
    if(0 != path_split(path, upper, last))
        return 1;

    inode_t *upperip = inode_get_dir(upper);
    if(upperip == NULL)
        return 1;

    if(0 != inode_mkdir(upperip, last))
        return 1;

    inode_return(upperip);

    return 0;
}

int rmdir(const char *path)
{
    if(0 != path_check(path))
        return 1;

    uint64_t pathlen = strlen(path);
    char upper[pathlen + 1], last[DIRNAME_MAX_LEN + 1];
    if(0 != path_split(path, upper, last))
        return 1;

    inode_t *upperip = inode_get_dir(upper);
    if(upperip == NULL)
        return 1;

    if(0 != inode_rmdir(upperip, last))
        return 1;

    inode_return(upperip);

    return 0;
}

int rmfile(const char *path)
{
    if(0 != path_check(path))
        return 1;

    uint64_t pathlen = strlen(path);
    char upper[pathlen + 1], last[DIRNAME_MAX_LEN + 1];
    if(0 != path_split(path, upper, last))
        return 1;

    inode_t *upperip = inode_get_dir(upper);
    if(upperip == NULL)
        return 1;

    if(0 != inode_rmfile(upperip, last))
        return 1;

    inode_return(upperip);

    return 0;
}

int file_exit(void)
{
    set_destroy(&pathset);

    struct fset *p = fset_head.next, *tmp;

    while(p){
        file_lock(p->fp);
        if(0 != inode_return(p->fp->ip)) return 1;
        free(p->fp);
        tmp = p;
        p = p->next;
        free(tmp);
    }

    return 0;
}
