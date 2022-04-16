#ifndef _ERROR_H
#define _ERROR_H

#include "types.h"

int elog(int type, char *msg, uint32_t len);

void panic(char *msg);

#endif
