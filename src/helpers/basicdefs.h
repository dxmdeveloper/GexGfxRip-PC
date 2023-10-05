#pragma once
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// typedef uint64_t u64;
// typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef uintptr_t uptr;

typedef struct u32pair_struct {
    uint32_t first;
    uint32_t second;
} u32pair;

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