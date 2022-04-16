#ifndef _SECURITY_H
#define _SECURITY_H

#include "types.h"

struct key128 {
    uchar k[16];
};

struct key256 {
    uchar k[32];
};

#endif
