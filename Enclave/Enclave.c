#include "Enclave.h"
#include "Enclave_t.h"
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "efs.h"

void ecall_hello_from_enclave(char *buf, size_t len)
{
    const char *hello = "Hello world";

    size_t size = len;
    if(strlen(hello) < len)
    {
        size = strlen(hello) + 1;
    }

    memcpy(buf, hello, size - 1);
    buf[size-1] = '\0';
}


FILE* fopen(const char *filename, const char *opentype){

}

int fclose(FILE *stream){

}

size_t fread(void *data, size_t size, size_t count, FILE *stream){

}

size_t fwrite(const void *data, size_t size, size_t count, FILE *stream){

}

int feof(FILE *stream){

}

long int ftell(FILE *stream){

}

int fseek(FILE *stream, long int offset, int whence){

}

