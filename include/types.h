#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

typedef unsigned char uchar;
typedef unsigned char uint8_t;
typedef unsigned short ushort;
typedef unsigned short uint16_t;
typedef unsigned int uint;
typedef unsigned int uint32_t;
typedef unsigned long ulong;
typedef unsigned long uint64_t;

typedef uint64_t size_t;

#ifndef UINT8_MAX
    #define UINT8_MAX 0xffU
#endif

#ifndef UINT16_MAX
    #define UINT16_MAX 0xffffU
#endif

#ifndef UINT32_MAX
    #define UINT32_MAX 0xffffffff
#endif

#ifndef UINT64_MAX
    #define UINT64_MAX 0xffffffffffffffff
#endif

#endif
