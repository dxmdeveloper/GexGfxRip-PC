
#ifndef _BINARY_PARSE_H_
#define _BINARY_PARSE_H_ 1
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/** @param n Number of objects in the dest array
    @returns Number of objects read successfully. */
size_t fread_LE_U32 (uint32_t dest[], size_t n, FILE * stream);

/** @param n Number of objects in the dest array
    @returns Number of objects read successfully. */
size_t fread_LE_I32 (int32_t dest[], size_t n,  FILE * stream);

/** @param n Number of objects in the dest array
    @returns Number of objects read successfully. */
size_t fread_LE_U16 (uint16_t dest[], size_t n, FILE * stream);

/** @param n Number of objects in the dest array
    @returns Number of objects read successfully. */
size_t fread_LE_I16 (int16_t dest[], size_t n,  FILE * stream);


uint32_t aob_read_LE_U32(const void * src);
int32_t aob_read_LE_I32 (const void * src);
uint16_t aob_read_LE_U16(const void * src);
int16_t aob_read_LE_I16 (const void * src);

#endif