#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "binary_parse.h"
#include "basicdefs.h"

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
        u32 __file_ex_bswap32(const u32 x){
            u8 tmp[4];
            tmp[0] = (u8)(x >> 0);
            tmp[1] = (u8)(x >> 8);
            tmp[2] = (u8)(x >> 16);
            tmp[3] = (u8)(x >> 24);
            return *(u32*)&tmp;
        }
        u16 __file_ex_bswap16(const u16 x){
            u8 tmp[2];
            tmp[0] = (u8)(x >> 0);
            tmp[1] = (u8)(x >> 8);
            return *(u16*)&tmp;
        }
        #define bswap32(x) __file_ex_bswap32(x)
        #define bswap16(x) __file_ex_bswap16(x)
    #endif
#endif


size_t fread_LE_U32 (u32 dest[], size_t n, FILE * stream){
    n = fread(dest, sizeof(u32), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = bswap32(dest[i]);
    #endif
    return n;
}
size_t fread_LE_I32 (i32 dest[], size_t n, FILE * stream){
    n = fread(dest, sizeof(i32), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = (i32) bswap32(dest[i]);
    #endif
    return n;
}
size_t fread_LE_U16 (u16 dest[], size_t n, FILE * stream) {
    n = fread(dest, sizeof(u16), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = bswap16(dest[i]);
    #endif
    return n;
}
size_t fread_LE_I16 (i16 dest[], size_t n, FILE * stream){
    n = fread(dest, sizeof(i16), n, stream);
    #ifdef __BIG_ENDIAN__
        for(size_t i = 0; i < n; i++)
            dest[i] = (i16) bswap16(dest[i]);
    #endif
    return n;
}

// --- ARRAY OF BYTES ---
uint32_t aob_read_LE_U32(const void * src){
    u32 val = *(uint32_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap32(&val);
    #endif
    return val;
}
int32_t aob_read_LE_I32(const void * src){
    i32 val = *(int32_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap32((u32*)&val);
    #endif
    return (i32)val;
}
uint16_t aob_read_LE_U16(const void * src){
    u16 val = *(uint16_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap16(&val);
    #endif
    return val;
}
int16_t aob_read_LE_I16(const void * src){
    i16 val = *(int16_t *)src;
    #ifdef __BIG_ENDIAN__ 
        bswap16((u16*)&val);
    #endif
    return (i16)val;
}

#undef bswap16
#undef bswap32