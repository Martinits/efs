#include "efs.h"
#include "error.h"
#include "sgx_trts.h"
#include "enclave_t.h"
#include "test_common.h"
#include <stdio.h>
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
uint8_t rands[30*1024] = {0};
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
        /*if(0 != rand(&pos, sizeof(pos))) return 1;*/
        /*pos %= sizeof(rands);*/
        pos = sizeof(rands) - i - 1;
        elog(LOG_DEBUG, "i = %d", i);
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
    elog(LOG_DEBUG, "test 5");

    memset(rands, 0x5a, sizeof(rands));

    file *fp = fopen("/bigfile", O_RDONLY);
    if(!fp) return 1;

    uint8_t check;
    uint32_t checkpos;

    for(int i = 0; i < 1000; i++){
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

static long getclock(void)
{
    long retval;
    ocall_getclock(&retval);
    return retval;
}

uint32_t file_sz; //in KB
uint32_t rw_unit; //in KB
uint32_t uints;
uint32_t rand_time;

static int test_read(void)
{
    file *fp = fopen("/bigfile", O_RDONLY);
    if(!fp) return 1;

    uint8_t buf[rw_unit * 1024];

    long start = getclock();

    for(uint32_t i = 0; i < file_sz/rw_unit; i++){
        if(sizeof(buf) != fread(fp, buf, sizeof(buf))) return 1;
    }

    long end = getclock();

    if(0 != fclose(fp)) return 1;

    double t = (double)(end - start) / 1000;

    elog(LOG_INFO, "read: file_sz: %d, rw_unit: %d, elapsed time: %.3f ms, throughput: %ld KB/s",
            file_sz, rw_unit, t, (ulong)((double)(file_sz*1000)/t));

    return 0;
}

static int test_write(void)
{
    file *fp = fopen("/bigfile", O_CREATE | O_RDWR);
    if(!fp) return 1;

    uint8_t buf[rw_unit * 1024];
    if(0 != rand(buf, sizeof(buf))) return 1;

    long start = getclock();

    for(uint32_t i = 0; i < file_sz/rw_unit; i++){
        if(sizeof(buf) != fwrite(fp, buf, sizeof(buf))) return 1;
    }

    long end = getclock();

    if(0 != fclose(fp)) return 1;

    double t = (double)(end - start) / 1000;

    elog(LOG_INFO, "write: file_sz: %d, rw_unit: %d, elapsed time: %.3f ms, throughput: %ld KB/s",
            file_sz, rw_unit, t, (ulong)((double)(file_sz*1000)/t));

    return 0;
}

static int test_random_read(void)
{
    file *fp = fopen("/bigfile", O_RDONLY);
    if(!fp) return 1;

    uint8_t buf[rw_unit * 1024];
    uint32_t pos;

    long start = getclock();

    for(int i = 0; i < rand_time; i++){
        if(0 != rand(&pos, sizeof(pos))) return 1;
        pos %= uints;
        if(0 != fseek(fp, pos*sizeof(buf), SEEK_SET)) return 1;
        if(sizeof(buf) != fread(fp, buf, sizeof(buf))) return 1;
    }

    long end = getclock();

    if(0 != fclose(fp)) return 1;

    double t = (double)(end - start) / 1000 / rand_time;

    elog(LOG_INFO, "randread: file_sz: %d, rw_unit: %d, elapsed time: %.3f ms, throughput: %ld KB/s",
            file_sz, rw_unit, t, (ulong)((double)(rw_unit*1000)/t));

    return 0;
}

static int test_random_write(void)
{
    file *fp = fopen("/bigfile", O_RDWR);
    if(!fp) return 1;

    uint8_t buf[rw_unit * 1024];
    if(0 != rand(buf, sizeof(buf))) return 1;
    uint32_t pos;

    long start = getclock();

    for(int i = 0; i < rand_time; i++){
        if(0 != rand(&pos, sizeof(pos))) return 1;
        pos %= uints;
        if(0 != fseek(fp, pos*sizeof(buf), SEEK_SET)) return 1;
        if(sizeof(buf) != fwrite(fp, buf, sizeof(buf))) return 1;
    }

    long end = getclock();

    if(0 != fclose(fp)) return 1;

    double t = (double)(end - start) / 1000 / rand_time;

    elog(LOG_INFO, "randwrite: file_sz: %d, rw_unit: %d, elapsed time: %.3f ms, throughput: %ld KB/s",
            file_sz, rw_unit, t, (ulong)((double)(rw_unit*1000)/t));

    return 0;

}

static int test_delay_read(void)
{
    file *fp = fopen("/bigfile", O_RDWR);
    if(!fp) return 1;

    uint8_t buf[BLK_SZ];
    uint32_t pos;

    long start, total = 0;

    if(0 != rand(&pos, sizeof(pos))) return 1;
    pos %= uints;
    if(0 != fseek(fp, pos*sizeof(buf), SEEK_SET)) return 1;
    start = getclock();
    if(sizeof(buf) != fread(fp, buf, sizeof(buf))) return 1;
    total += getclock() - start;

    if(0 != fclose(fp)) return 1;

    double t = (double)(total) / 1000;

    elog(LOG_INFO, "readdelay: file_sz: %d, rw_unit: 4KB, delay: %.3f ms",
            file_sz, rw_unit, t);

    return 0;
}

static int test_delay_write(void)
{
    file *fp = fopen("/bigfile", O_RDWR);
    if(!fp) return 1;

    uint8_t buf[BLK_SZ];
    if(0 != rand(buf, sizeof(buf))) return 1;
    uint32_t pos;

    long start, total = 0;

    if(0 != rand(&pos, sizeof(pos))) return 1;
    pos %= uints;
    if(0 != fseek(fp, pos*sizeof(buf), SEEK_SET)) return 1;
    start = getclock();
    if(sizeof(buf) != fwrite(fp, buf, sizeof(buf))) return 1;
    total += getclock() - start;

    if(0 != fclose(fp)) return 1;

    double t = (double)(total) / 1000;

    elog(LOG_INFO, "writedelay: file_sz: %d, rw_unit: 4KB, delay: %.3f ms",
            file_sz, rw_unit, t);

    return 0;
}

uint32_t icac_miss = 0, icac_access = 0, bcac_access = 0, bcac_miss = 0;

static int test_build_small_files(void)
{
    uint8_t buf[BLK_SZ];
    if(0 != rand(buf, sizeof(buf))) return 1;

    for(int i = 0; i < 10000; i++){
        char fname[20] = {0};
        snprintf(fname, sizeof(fname), "/file%04d", i);
        file *fp = fopen(fname, O_CREATE | O_RDWR);
        if(!fp) return 1;
        if(sizeof(buf) != fwrite(fp, buf, sizeof(buf))) return 1;
        if(0 != fclose(fp)) return 1;
    }
    return 0;
}

//13
static int test_cache_miss_rate_big(void)
{
    file *fp = fopen("/bigfile", O_RDONLY);
    if(!fp) return 1;

    uint8_t buf[BLK_SZ];
    uint32_t pos;

    for(int i = 0; i < rand_time; i++){
        if(0 != rand(&pos, sizeof(pos))) return 1;
        pos %= uints;
        if(0 != fseek(fp, pos*sizeof(buf), SEEK_SET)) return 1;
        if(sizeof(buf) != fread(fp, buf, sizeof(buf))) return 1;
    }

    if(0 != fclose(fp)) return 1;

    elog(LOG_INFO, "cachemissrate_big: icac_access: %d, icac_miss: %d, icac_miss_rate: %.3f",
            icac_access, icac_miss, (double)(icac_miss)/icac_access);
    elog(LOG_INFO, "cachemissrate_big: bcac_access: %d, bcac_miss: %d, bcac_miss_rate: %.3f",
            bcac_access, bcac_miss, (double)(bcac_miss)/bcac_access);

    return 0;
}

static int test_cache_miss_rate_small(void)
{
    uint8_t buf[BLK_SZ];
    uint32_t fnum, pos;

    for(int i = 0; i < rand_time; i++){
        if(0 != rand(&fnum, sizeof(fnum))) return 1;
        fnum %= 10000;

        char fname[20] = {0};
        snprintf(fname, sizeof(fname), "/file%04d", fnum);
        file *fp = fopen(fname, O_RDONLY);
        if(!fp) return 1;

        if(sizeof(buf) != fread(fp, buf, sizeof(buf))) return 1;

        if(0 != fclose(fp)) return 1;
    }

    elog(LOG_INFO, "cachemissrate_small: icac_access: %d, icac_miss: %d, icac_miss_rate: %.3f",
            icac_access, icac_miss, (double)(icac_miss)/icac_access);
    elog(LOG_INFO, "cachemissrate_small: bcac_access: %d, bcac_miss: %d, bcac_miss_rate: %.3f",
            bcac_access, bcac_miss, (double)(bcac_miss)/bcac_access);

    return 0;
}

static int (*efs_testers[NTESTERS])(void) = {
    efs_test_0,
    efs_test_1,
    efs_test_2,
    efs_test_3,
    efs_test_4,
    efs_test_5,
    test_read,
    test_write,
    test_random_read,
    test_random_write,
    test_delay_read,
    test_delay_write,
    test_build_small_files,
    test_cache_miss_rate_big,
    test_cache_miss_rate_small,
    NULL,
};

int ecall_efs_test(int n, uint32_t filesz, uint32_t rwunit, uint32_t randtime)
{
    if(n >= NTESTERS || efs_testers[n] == NULL) return 1;

    int retval;
    uint8_t ikh[IKH_SZ] = {0};
    key128_t *iv = (key128_t *)ikh, *key = iv + 1;
    key256_t *hash = (key256_t *)(key + 1);

    if(SGX_SUCCESS != ocall_tester_get_ikh(&retval, ikh) || retval != 0)
        return 1;

    if(0 != efs_init(iv, key, hash, BACKEND_TP_FILE)) return 1;

    file_sz = filesz;
    rw_unit = rwunit;
    uints = file_sz / rw_unit;
    rand_time = randtime;

    if(0 != efs_testers[n]()) return 1;

    if(efs_exit(iv, key, hash) != 0) return 1;

    /*if(0 != efs_init(iv, key, hash, BACKEND_TP_FILE)) return 1;*/

    /*if(0 != efs_testers[5]()) return 1;*/

    /*if(efs_exit(iv, key, hash) != 0) return 1;*/

    if(SGX_SUCCESS != ocall_tester_set_ikh(&retval, ikh) || retval != 0)
        return 1;

    return 0;
}
