#ifndef DPP_TYPES_H
#define DPP_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float       f32;
typedef double      f64;
typedef long double f96;

#define m_align(x, a) (((x) + (a) - 1) & ~((a) - 1))

enum dpp_type_kind {
    TYPE_INVALID,
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_INT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_ACCUM,
    TYPE_UACCUM,
    TYPE_DECIMAL,
    TYPE_EXT_INT
};

#endif
