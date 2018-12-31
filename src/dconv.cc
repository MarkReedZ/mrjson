
#ifndef RAPIDJSON_UINT64_C2
#define RAPIDJSON_UINT64_C2(high32, low32) ((static_cast<uint64_t>(high32) << 32) | static_cast<uint64_t>(low32))
#endif

#if defined(_MSC_VER)

//#if __WORDSIZE == 64
//# ifndef __intptr_t_defined
//typedef long int                intptr_t;
//#  define __intptr_t_defined
//# endif
//typedef unsigned long int        uintptr_t;
//#else
//# ifndef __intptr_t_defined
//typedef int                        intptr_t;
//#  define __intptr_t_defined
//# endif
//typedef unsigned int                uintptr_t;
//#endif

#if __WORDSIZE == 64
typedef long int                intmax_t;
typedef unsigned long int        uintmax_t;
#else
typedef long long int                intmax_t;
typedef unsigned long long int        uintmax_t;
#endif

#ifndef __int8_t_defined
# define __int8_t_defined
typedef signed char                int8_t;
typedef short int                int16_t;
typedef int                        int32_t;
# if __WORDSIZE == 64
typedef long int                int64_t;
# else
typedef long long int                int64_t;
# endif
#endif

typedef unsigned char                uint8_t;
typedef unsigned short int        uint16_t;
typedef unsigned int                uint32_t;
#if __WORDSIZE == 64
typedef unsigned long int        uint64_t;
#else
typedef unsigned long long int        uint64_t;
#endif

#else
#include <stdint.h>
#endif

#include <cstdlib>
#include <cstring>
//#include <inttypes.h>
#include "itoa.h"
#include "dtoa.h"
#include "diyfp.h"
#include "ieee754.h"

