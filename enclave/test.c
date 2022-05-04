#include "efs.h"
#include "sgx_trts.h"
#include "enclave_t.h"
#include "test_common.h"
#include <string.h>

#define HELLO ("hello, world!")

int ecall_efs_test(void)
{
    int retval;
    uint8_t ikh[IKH_SZ] = {0};
    key128_t *iv = (key128_t *)ikh, *key = iv + 1;
    key256_t *hash = (key256_t *)(key + 1);

    if(SGX_SUCCESS != ocall_tester_get_ikh(&retval, ikh) || retval != 0)
        return 1;

    if(0 != efs_init(iv, key, hash, BACKEND_TP_FILE)) return 1;

    file *fp = fopen("/newfile", O_RDWR | O_CREATE);
    if(!fp) return 1;

    char buf[100] = HELLO;
    uint32_t buflen = strlen(buf) + 1;
    if(buflen != fwrite(fp, buf, buflen)) return 1;

    if(0 != fclose(fp)) return 1;

    fp = fopen("/newfile", O_RDONLY);
    if(!fp) return 1;
    memset(buf, 0, sizeof(buf));

    if(buflen != fread(fp, buf, buflen)) return 1;

    if(strcmp(buf, HELLO)) return 1;

    if(0 != fclose(fp)) return 1;

    if(efs_exit(iv, key, hash) != 0) return 1;

    if(SGX_SUCCESS != ocall_tester_set_ikh(&retval, ikh) || retval != 0)
        return 1;

    return 0;
}
