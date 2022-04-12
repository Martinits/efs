#ifndef _EFS_H
#define _EFS_H

#include "types.h"

struct FILE {

};

FILE* fopen(const char *filename, const char *opentype);

int fclose(FILE *stream);

size_t fread(void *data, size_t size, size_t count, FILE *stream);

size_t fwrite(const void *data, size_t size, size_t count, FILE *stream);

int feof(FILE *stream);

long int ftell(FILE *stream);

int fseek(FILE *stream, long int offset, int whence);

#endif
