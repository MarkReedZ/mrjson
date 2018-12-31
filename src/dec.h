#pragma once

#if defined(_MSC_VER)
#define inline __inline
//#else
//#include <stdint.h>
#endif
#include <stddef.h>
#include <assert.h>
#include "Python.h"

typedef char bool;
enum { false, true };

enum JsonTag {
    JSON_NUMBER = 0,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL = 0xF
};
typedef enum JsonTag JsonTag;

PyObject *jsonParse(char *str, char **endptr, size_t len); //, char **endptr);
#ifdef __AVX2Z__
PyObject *jParse(char *str, int length, char **endptr, JsonValue *value);//, JsonAllocator &allocator);
#endif

