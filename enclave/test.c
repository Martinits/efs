#include "efs.h"
#include "error.h"
#include "sgx_trts.h"
#include "enclave_t.h"
#include "test_common.h"
#include <string.h>

#define HELLO ("hello, world!")
#define HELLOLEN (14)

static int efs_test_0(void)
{
    file *fp = fopen("/newfile", O_RDWR | O_CREATE);
    if(!fp) return 1;

    char buf[100] = HELLO;
    if(HELLOLEN != fwrite(fp, buf, HELLOLEN)) return 1;

    if(0 != fclose(fp)) return 1;

    return 0;
}

static int efs_test_1(void)
{
    file *fp = fopen("/newfile", O_RDONLY);
    if(!fp) return 1;

    char buf[HELLOLEN] = {0};

    if(HELLOLEN != fread(fp, buf, HELLOLEN)) return 1;

    if(strcmp(buf, HELLO)) return 1;

    if(0 != fclose(fp)) return 1;

    return 0;
}

static int rand(void *buf, uint32_t len){
    return SGX_SUCCESS != sgx_read_rand(buf, len);
}

// fseek ftell feof
static int efs_test_2(void)
{
    file *fp = fopen("/testfile", O_CREATE | O_RDWR);
    if(!fp) return 1;

    uint8_t buf[4096] = {0}, test;

    if(0 != rand(buf, sizeof(buf))) return 1;

    if(sizeof(buf) != fwrite(fp, buf, sizeof(buf))) return 1;

    if(0 != fclose(fp)) return 1;

    fp = fopen("/testfile", O_RDONLY);
    if(!fp) return 1;

    uint32_t pos;
    for(int i = 0; i < 100; i++){
        if(0 != rand(&pos, sizeof(uint32_t))) return 1;

        pos %= sizeof(buf);

        if(0 != fseek(fp, pos, SEEK_SET)) return 1;

        if(1 != fread(fp, &test, 1)) return 1;

        if(test != buf[pos]) return 1;
    }

    if(0 != rand(&pos, sizeof(uint32_t))) return 1;
    pos %= sizeof(buf);
    if(0 != fseek(fp, pos, SEEK_SET)) return 1;
    uint32_t newpos;
    if(0 != rand(&newpos, sizeof(newpos))) return 1;
    newpos %= sizeof(buf)-pos;
    if(0 != fseek(fp, newpos, SEEK_CUR)) return 1;
    newpos += pos;
    if(newpos != ftell(fp)) return 1;
    if(1 != fread(fp, &test, 1)) return 1;
    if(test != buf[newpos]) return 1;

    if(0 != fseek(fp, 0, SEEK_END)) return 1;
    if(!feof(fp)) return 1;

    if(0 != fclose(fp)) return 1;

    return 0;
}

//mkdir rmdir rmfile
static int efs_test_3(void)
{
    file *fp = fopen("/file1", O_CREATE | O_RDWR);
    if(!fp) return 1;
    if(0 != fclose(fp)) return 1;

    if(0 != mkdir("/dir1")) return 1;
    if(0 == mkdir("/dir1")) return 1;
    if(0 != mkdir("/dir2")) return 1;
    if(0 != mkdir("/dir1/dir11")) return 1;
    if(0 != mkdir("/dir3")) return 1;
    if(0 != rmdir("/dir3")) return 1;

    fp = fopen("/dir1/dir11/file111", O_CREATE | O_RDWR);
    if(!fp) return 1;
    if(0 != fclose(fp)) return 1;

    if(0 == rmdir("/dir1/dir11")) return 1;

    if(0 != rmfile("/dir1/dir11/file111")) return 1;

    return 0;
}

// bigfile
uint8_t rands[1024] = {0};
static int efs_test_4(void)
{
    file *fp = fopen("/bigfile", O_CREATE | O_RDWR);
    if(!fp) return 1;

    uint8_t buf[BLK_SZ] = {0};
    /*if(0 != rand(rands, sizeof(rands))) return 1;*/
    memset(rands, 0x5a, sizeof(rands));
    for(uint32_t i = 0; i < sizeof(rands); i++){
        elog(LOG_DEBUG, "%d", i);
        memset(buf, rands[i], sizeof(buf));
        if(sizeof(buf) != fwrite(fp, buf, sizeof(buf))) return 1;
    }

    if(0 != fclose(fp)) return 1;

    fp = fopen("/bigfile", O_RDONLY);
    if(!fp) return 1;

    uint32_t pos;
    for(int i = 0; i < 1000; i++){
        if(0 != rand(&pos, sizeof(pos))) return 1;
        pos %= sizeof(rands);
        if(0 != fseek(fp, pos*sizeof(buf), SEEK_SET)) return 1;
        if(sizeof(buf) != fread(fp, buf, sizeof(buf))) return 1;
        uint j = sizeof(buf);
        while(j--){
            if(buf[j] != rands[pos]) return 1;
        }
    }

    return 0;
}

static int efs_test_5(void)
{
    elog(LOG_INFO, "test 5");

    memset(rands, 0x5a, sizeof(rands));

    file *fp = fopen("/bigfile", O_RDONLY);
    if(!fp) return 1;

    uint8_t check;
    uint32_t checkpos;

    for(int i = 0; i < 10000; i++){
        if(0 != rand(&checkpos, sizeof(checkpos))) return 1;
        checkpos %= sizeof(rands);
        elog(LOG_DEBUG, "check %d: block: %d", i, checkpos);
        if(0 != fseek(fp, checkpos * BLK_SZ, SEEK_SET)) return 1;
        if(1 != fread(fp, &check, 1)) return 1;
        if(check != rands[checkpos]) return 1;
    }

    if(0 != fclose(fp)) return 1;

    return 0;
}

static int (*efs_testers[NTESTERS])(void) = {
    efs_test_0,
    efs_test_1,
    efs_test_2,
    efs_test_3,
    efs_test_4,
    efs_test_5,
    NULL,
};

int ecall_efs_test(int n)
{
    if(n >= NTESTERS || efs_testers[n] == NULL) return 1;

    int retval;
    uint8_t ikh[IKH_SZ] = {0};
    key128_t *iv = (key128_t *)ikh, *key = iv + 1;
    key256_t *hash = (key256_t *)(key + 1);

    if(SGX_SUCCESS != ocall_tester_get_ikh(&retval, ikh) || retval != 0)
        return 1;

    if(0 != efs_init(iv, key, hash, BACKEND_TP_FILE)) return 1;

    if(0 != efs_testers[n]()) return 1;

    if(efs_exit(iv, key, hash) != 0) return 1;

    /*if(0 != efs_init(iv, key, hash, BACKEND_TP_FILE)) return 1;*/

    /*if(0 != efs_testers[5]()) return 1;*/

    /*if(efs_exit(iv, key, hash) != 0) return 1;*/

    if(SGX_SUCCESS != ocall_tester_set_ikh(&retval, ikh) || retval != 0)
        return 1;

    return 0;
}
