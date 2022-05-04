#include "types.h"
#include "enclave_u.h"
#include <stdio.h>
#include <string.h>
#include "sgx_urts.h"
#include "efs_common.h"
#include "test_common.h"

#define EFS_DISK_NAME ("efsdisk")
#define EFS_IKH ("efsikh")

int ocall_tester_get_ikh(uint8_t *buf){
    FILE *fp = fopen(EFS_IKH, "r");
    if(!fp) return 1;

    if(1 != fread(buf, IKH_SZ, 1, fp)) return 1;

    fclose(fp);

    return 0;
}

int ocall_tester_set_ikh(uint8_t *buf){
    FILE *fp = fopen(EFS_IKH, "w");
    if(!fp) return 1;

    if(1 != fwrite(buf, IKH_SZ, 1, fp)) return 1;

    fclose(fp);

    return 0;
}
