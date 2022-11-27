#pragma once
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;
typedef double f64;

#ifdef _WIN32
    #define PATH_SEP "\\"
#else
    #define PATH_SEP "/"
#endif

// detailed error log
#ifdef DEBUG
    #define dbg_errlog(str, ...) (fprintf(stderr, str, __VA_ARGS__))
#else
    #define dbg_errlog(str, ...)
#endif