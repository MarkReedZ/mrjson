
#ifndef RAPIDJSON_UINT64_C2
#define RAPIDJSON_UINT64_C2(high32, low32) ((static_cast<uint64_t>(high32) << 32) | static_cast<uint64_t>(low32))
#endif

#include <cstdlib>
#include <cstring>
#include <stdint.h>
//#include <inttypes.h>
#include "itoa.h"
#include "dtoa.h"
#include "diyfp.h"
#include "ieee754.h"

