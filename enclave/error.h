#ifndef _ERROR_H
#define _ERROR_H

#include "types.h"
#include "log_types.h"

int elog(int type, const char *msg, ...);

void panic(const char *msg, ...);

#endif
