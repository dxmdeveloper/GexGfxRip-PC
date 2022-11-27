#include <stdlib.h>
#include <stdio.h>
#include "filescanning.h"
#include "basicdefs.h"
#include "gfx.h"

// mkdir / stat
#ifdef _WIN32
    #include "dirent.h"
#else //POSIX
    #include <sys/stat.h>
#endif

// PRIVATE DECLARATIONS:
uintptr_t findU32(void *startPtr, void *endPtr, u32 ORMask, u32 matchVal);
uintptr_t infilePtrToOffset(u32 infile_ptr, uintptr_t startOffset);
u32 offsetToInfilePtr(uintptr_t fileOffset, uintptr_t startOffset);
void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);
void scanChunk(void *startOffset, void *endOffset, scan_foundCallback_t, char path[], u32 pathLen, uintptr_t inf_fileDataAlloc);

// PUBLIC DEFINITIONS:
void scan4Gfx(char filename[], scan_foundCallback_t foundCallback){
    void *fileData;
    FILE *file;
    size_t fileSize;
    
    file = fopen(filename, "rb");
    if(file == NULL) {

        fprintf(stderr, "Err: Cannot open file\n");
        return;
    }

    // check file size
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    rewind(file);

    if(fileSize < 16){
        fprintf(stderr, "Err: File is too small\n");
        fclose(file);
        return;
    }

    // read file into memory
    fileData = malloc(fileSize);
    if(fileData == NULL){
        fprintf(stderr, "Err: Out of memory (filescanning.c: scan4Gfx)\n");
        fclose(file);
        exit(0x6d656d); // mem
    }
    fread(fileData, 1, fileSize, file);
    fclose(file);

    // Create directory for graphics.
    char *path = malloc(strlen(filename) + 27); // 26 chars for filename reserved
    strcpy(path, filename);
    strcat(path, "-rip"); // "{filename}-rip"

    #ifdef _WIN32
        _mkdir(path);
    #else // POSIX
        mkdir(path, 0777);
    #endif


    // --- Actual Scanning Process ---    
    u32 pathLen = strlen(path);
    // check first 4 bytes of file
    if(((u32*)fileData)[0] == 0 || ((u32*)fileData)[0] > 255){
        // TYPE OF FILE: one chunk
        scanChunk(fileData, fileData + fileSize, foundCallback, path, pathLen, fileData);
    } else {
        // TYPE OF FILE: chunked archive
        // Too small archive error
        if(fileSize < 32 + ((u32*)fileData)[0] * 16){
            free(path);
            fprintf(stderr, "Err: Invalid archive file\n");
            fclose(file);
            return;
        }
        for(u32 chunkIndex = 0; chunkIndex < ((u32*)fileData)[0]; chunkIndex++){
            u32 chunkOffset = ((u32*)fileData)[4 + chunkIndex*4 + 3];
            u32 chunkSize   = ((u32*)fileData)[4 + chunkIndex*4 + 2];

            // skipping dummy chunks
            if(chunkSize < 24) continue;
            // chunk offset is not in file error
            if(chunkOffset + chunkSize > fileSize){
                fprintf(stderr, "invalid archive chunk allocation [chunk skipped]\n");
                continue;
            }

           scanChunk(fileData+chunkOffset, fileData+chunkOffset+chunkSize, foundCallback, path, pathLen, fileData);
        }
    }

    // END OF FUNCTION
    // clean
    free(fileData);
    free(path);
}

void scanChunk(void *startOffset, void *endOffset, scan_foundCallback_t foundCallback, char path[], u32 pathLen, uintptr_t inf_fileDataAlloc){
    struct gfx_palette palette;
    void *lastPalPtr = NULL;
    void *palp = NULL;
    void *gfx = startOffset + 15;
    
    while(true){
        // find next graphic
        gfx = findU32(gfx + 1, endOffset - 24, 0x9f05, 0xffff9985);
        if(gfx == NULL) break;

        // TODO: checking for more palettes assigned
        {
            // match palette
            palp = matchColorPalette(gfx - 16, startOffset, endOffset);
            // palette not found
            if(palp == NULL){
                // placeholder
            }
            // create palette
            else if(palp != lastPalPtr){
                palette = gfx_createPalette(palp);
                lastPalPtr = palp;
            }

            // filename
            struct gex_gfxHeader *gfxHeader = gfx - 16;
            char filename[18];
            sprintf(filename, PATH_SEP"%c%c-%08X.png",
                    ((gfxHeader->typeSignature & 4) == 4 ? 'S' : 'B'),
                    ((gfxHeader->typeSignature & 1) == 1 ? '8' : '4'),
                    (u32)((uintptr_t)gfx - 16 - inf_fileDataAlloc));
            strncpy(path+pathLen, filename, 18);

            // call callback function
            foundCallback((void*) gfx - 16, (palette.colorsCount == 0 ? NULL : &palette), path);
        }
    }
}


// PRIVATE DEFINITIONS:
uintptr_t infilePtrToOffset(u32 infile_ptr, uintptr_t startOffset){
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

u32 offsetToInfilePtr(uintptr_t offset, uintptr_t startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}

// TODO: DOCS
void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd){
    u32 *p2p2Palette = chunkStart - 1;
    // loop breaks when pal is not found or when found a valid palette
    while(true){
        p2p2Palette = findU32(p2p2Palette + 1, chunkEnd - 4, 0, offsetToInfilePtr(gfxOffset, chunkStart)) + 4;
        if ((uptr) p2p2Palette == 4 ) return NULL;

        u32 *palp = infilePtrToOffset(*p2p2Palette, chunkStart);
        // pointer verification
        if (!((uptr)palp < (uptr)chunkStart || (uptr)palp > (uptr)chunkEnd - 36) && (*palp | 0xff01) == 0xffffff01) break;
    }
    
    return infilePtrToOffset(*p2p2Palette, chunkStart);
}

/// @brief function scanning memory for u32 value
/// @param ORMask logical OR mask. 0 by default
/// @param matchVal searched value.
/// @return offset of found value. null if not found.
uintptr_t findU32(void *startPtr, void *endPtr, u32 ORMask, u32 matchVal){
    endPtr -= (endPtr - startPtr) % 4;
    while(startPtr < endPtr){
        if((ORMask | *((u32*)(startPtr))) == (ORMask | matchVal)){
            return (uintptr_t)startPtr;
        }

        startPtr++;
    }
    return 0;
}