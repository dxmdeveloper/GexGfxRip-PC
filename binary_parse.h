
#ifndef _BINARY_PARSE_H_
#define _BINARY_PARSE_H_ 1

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

// TODO : BIG ENDIAN TESTS
#ifdef __BIG_ENDIAN__  
    #if defined(_MSC_VER)
        #define bswap32(x) _byteswap_ulong(x)
        #define bswap16(x) _byteswap_ushort(x)
    #elif  (defined(__clang__) && __has_builtin(__builtin_bswap16)) \
        || (defined(__GNUC__ ) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
        #define bswap32(x) __builtin_bswap32(x)
        #define bswap16(x) __builtin_bswap16(x)
    #else 
        inline static uint32_t __file_ex_bswap32(const uint32_t x){
            uint8_t tmp[4];
            tmp[0] = (uint8_t)(x >> 0);
            tmp[1] = (uint8_t)(x >> 8);
            tmp[2] = (uint8_t)(x >> 16);
            tmp[3] = (uint8_t)(x >> 24);
            return *(uint32_t*)&tmp;
        }
        inline static uint16_t __file_ex_bswap16(const uint16_t x){
            u8 tmp[2];
            tmp[0] = (uint8_t)(x >> 0);
            tmp[1] = (uint8_t)(x >> 8);
            return *(uint16_t*)&tmp;
        }
        #define bswap32(x) __file_ex_bswap32(x)
        #define bswap16(x) __file_ex_bswap16(x)
    #endif
#endif


inline static size_t fread_LE_U32 (uint32_t dest[], size_t n, FILE * stream){
    n = fread(dest, sizeof(uint32_t), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = bswap32(dest[i]);
    #endif
    return n;
}
inline static size_t fread_LE_I32 (int32_t dest[], size_t n, FILE * stream){
    n = fread(dest, sizeof(int32_t), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = (int32_t) bswap32(dest[i]);
    #endif
    return n;
}
inline static size_t fread_LE_U16 (uint16_t dest[], size_t n, FILE * stream) {
    n = fread(dest, sizeof(uint16_t), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = bswap16(dest[i]);
    #endif
    return n;
}
inline static size_t fread_LE_I16 (int16_t dest[], size_t n, FILE * stream){
    n = fread(dest, sizeof(int16_t), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = (int16_t) bswap16(dest[i]);
    #endif
    return n;
}

// --- ARRAY OF BYTES ---
inline static uint32_t aob_read_LE_U32(const void * src){
    uint32_t val = *(uint32_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap32(&val);
    #endif
    return val;
}
inline static int32_t aob_read_LE_I32(const void * src){
    int32_t val = *(int32_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap32((uint32_t*)&val);
    #endif
    return (int32_t)val;
}
inline static uint16_t aob_read_LE_U16(const void * src){
    uint16_t val = *(uint16_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap16(&val);
    #endif
    return val;
}
inline static int16_t aob_read_LE_I16(const void * src){
    int16_t val = *(int16_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap16((uint16_t*)&val);
    #endif
    return (int16_t)val;
}

#undef bswap16
#undef bswap32

#endif