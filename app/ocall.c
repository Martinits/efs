#include "types.h"
#include "enclave_u.h"
#include <stdio.h>
#include <string.h>
#include "sgx_urts.h"
#include "entry.h"

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

static int print(int file, char *msg, uint32_t len)
{
    if(file != 1 && file != 2) return 1;

    char *buf = (char *)malloc(sizeof(char)*(len+1));
    memcpy(buf, msg, len*sizeof(char));
    buf[len] = 0;

    if(file == 1){
        fprintf(stdout, "%s", msg);
    }else
        fprintf(stderr, "%s", msg);

    free(buf);

    return 0;
}

int ocall_print(int file, char *msg, uint32_t len)
{
    return print(file, msg, len);
}

void ocall_enclave_panic(char *msg, uint32_t len, int exitcode)
{
    print(2, msg, len);
    fprintf(stderr, "Enclave exit with code %d\n", exitcode);

    if(SGX_SUCCESS == sgx_destroy_enclave(global_eid)){
        fprintf(stdout, "Enclave %d destroyed\n", global_eid);
    }else{
        fprintf(stdout, "Cannot destroy enclave %d\n", global_eid);
    }
}
