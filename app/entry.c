#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <pwd.h>
#define MAX_PATH FILENAME_MAX

#include "sgx_urts.h"
#include "entry.h"
#include "enclave_u.h"

extern int ocall_init(void);

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

int initialize_enclave(void)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    /* 调用 sgx_create_enclave 创建一个 Enclave 实例 */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        printf("Failed to create enclave, ret code: %d\n", ret);
        return -1;
    }

    return 0;
}

int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);

    const size_t max_buf_len = 100;
    char buffer[max_buf_len];
    memset(buffer, 0, sizeof(buffer));

    if(0 != ocall_init())
        return -1;

    if(initialize_enclave() < 0){
        printf("Enter a character before exit ...\n");
        getchar();
        return -1;
    }

    int retval;
    if(ecall_efs_test(global_eid, &retval) != SGX_SUCCESS || retval != 0)
        printf("efs_init fail with %d\n", retval);
    else printf("efs_init ok\n");

    sgx_destroy_enclave(global_eid);

    printf("Info: SampleEnclave successfully returned.\n");

    return 0;
}
