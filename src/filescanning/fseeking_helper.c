#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../helpers/binary_parse.h"
#include <ctype.h>

const char * strFindScopeEndFromInside(const char str[], char openChar, char closeChar){
    for(u32 openChCnt = 0; openChCnt || *str != closeChar; str++){
        if(*str == '\0') return NULL;
        else if(*str == openChar) openChCnt++;
        else if(*str == closeChar) openChCnt--;
    }
    return str;
}

const char * strFindScopeEnd(const char str[], char closeChar){
    char openChar = *str++;
    return strFindScopeEndFromInside(str, openChar, closeChar);
}

/** @brief checks if str1 starts with str2. Function is case insensive.
  * @return 1 if str1 starts with str2. 0 otherwise */
static int strstartswith_ci(const char * restrict str1, const char * restrict str2){
    while(*str2){
        if(!*str1 || (toupper(*str1) != toupper(*str2))) return 0;
        str1++; str2++;
    }
    return 1;
}

/** @brief checks if str1 starts with str2. Function is case sensive.
  * @return 1 if str1 starts with str2. 0 otherwise */
static int strstartswith_cs(const char * restrict str1, const char * restrict str2){
    while(*str2){
        if(!*str1 || (*str1 != *str2)) return 0;
        str1++; str2++;
    }
    return 1;
}

/** @brief rewrites string without white spaces, but no more than n bytes (including null terminator)
  * @return same pointer as dest param */
static inline char * str_rewrite_without_whitespaces(char * dest, const char * src, size_t n){
    char *d = dest;
    for(size_t i = 0; i < n-1 && *src; i++){
        if(!isspace(*src)) *d++ = *src; src++;
    }
    *d = '\0';
    return dest;
}

static inline void _fsmod_follow_pattern_read(const char ** pcurp, FILE * fp, jmp_buf * error_jmp_buf){
    unsigned long rcount = 0;
    char priformat[16] = "\0";

    if((*pcurp)[1] != '{') return;
    (*pcurp) += 2;
    const char * expr_end = strFindScopeEndFromInside((*pcurp), '{', '}');

    // output param
    if(strstartswith_ci((*pcurp), "stdout")
    || strstartswith_ci((*pcurp), "print" )){
        printf("%0lX:\n", ftell(fp));
        if(!((*pcurp)=strchr((*pcurp), ':'))) {(*pcurp) = expr_end; return;}
        // how many values to read param
        rcount = strtoul(++(*pcurp), NULL, 0);
        if(!((*pcurp)=strchr((*pcurp), ':'))) {(*pcurp) = expr_end; return;}

        // format param
        uint bytes = (uint)atoi((++(*pcurp))+1) / 8;
        if(bytes==0) bytes = 1;
        bool isSigned = false;

        switch(**pcurp){
            case 'X': sprintf(priformat,"%%0%uX ", bytes*2); break;
            case 'x': sprintf(priformat,"%%0%ux ", bytes*2); break;
            case 'u': case 'U': strcpy(priformat,"%u ");     break;
            case 'i': case 'I': strcpy(priformat,"%i "); isSigned = true; break;
        }
        if(strchr((*pcurp), 'n') && strchr((*pcurp), 'n') < expr_end){
            strcat(priformat, "\n"); 
        }

        void * buf = calloc(rcount, bytes);
        switch (bytes) {
            case 1: fread(buf, 1, rcount, fp); break;
            case 2: fread_LE_U16(buf, rcount, fp); break;
            case 4: fread_LE_U32(buf, rcount, fp); break;
        }
        for(unsigned long i = 0; i < rcount; i++){
            switch (bytes) {
                case 1: if(isSigned) printf(priformat, ((i8*)buf)[i]);
                        else printf(priformat, ((u8*)buf)[i]); break;
                case 2: if(isSigned) printf(priformat, ((i16*)buf)[i]);
                        else printf(priformat, ((u16*)buf)[i]); break;
                case 4: if(isSigned) printf(priformat, ((i32*)buf)[i]);
                        else printf(priformat, ((u32*)buf)[i]); break;
            }
        }
        printf("\n");
        if(buf) free(buf);
    }
    *pcurp = expr_end;
    // TODO: INTERNAL Vars
}


int fsmod_follow_pattern(fsmod_file_chunk * fch, const char pattern[], jmp_buf * error_jmp_buf){
    u32 u32val = 0;
    int intVal = 0;

    if(pattern[0] == 'e') {
        fseek(fch->ptrsFp, fch->ep, SEEK_SET);
        pattern+=1;
    }
    for(const char* pcur = pattern; *pcur; pcur++){
        switch(*pcur){
            case 'g':{
                u32val = fsmod_read_infile_ptr(fch->ptrsFp, fch->offset, error_jmp_buf);
                if(!u32val) return EXIT_FAILURE;
                fseek(fch->ptrsFp, u32val, SEEK_SET);
            } break;
            case '+':
            case '-': {
                intVal = atoi(pcur);
                fseek(fch->ptrsFp, intVal, SEEK_CUR);
            } break;
            case 'd':{
                //set data fp to gexptr offset read by ptrsFp
                u32 offset = fsmod_read_infile_ptr(fch->ptrsFp, fch->offset, error_jmp_buf);
                if(!offset) return EXIT_FAILURE;
                fseek(fch->dataFp, offset, SEEK_SET);       
            } break; 
            case 'r': _fsmod_follow_pattern_read(&pcur, fch->ptrsFp, error_jmp_buf); break;
        }
    }
    return EXIT_SUCCESS;
}

// TODO: TESTS AND FIXES
size_t fsmod_follow_pattern_recur(fsmod_file_chunk * fch, const char pattern[], void * pass2cb,
                                  int cb(fsmod_file_chunk * fch, gexdev_u32vec * iterVecp, u32 internalVars[INTERNAL_VAR_CNT], void * clientp),
                                  jmp_buf ** errbufpp){
    Stack32 offsetStack = {0};
    Stack32 loopStack = {0};
    gexdev_u32vec iterVec = {0};
    size_t cbCalls = 0;
    u32 internalVars[INTERNAL_VAR_CNT] = {0};
    char procpattern[257];
    char *procpatternp = procpattern;

    gexdev_u32vec_init_capcity(&iterVec, 4);
    Stack32_init(&offsetStack, 128);
    Stack32_init(&loopStack, 256);

    FSMOD_ERRBUF_CHAIN_ADD(*errbufpp,
        Stack32_close(&offsetStack);
        Stack32_close(&loopStack);
        gexdev_u32vec_close(&iterVec);
    );

    str_rewrite_without_whitespaces(procpattern, pattern, 257);

    if(pattern[0] == 'e') {
        fseek(fch->ptrsFp, fch->ep, SEEK_SET);
        procpatternp+=1;
    }
    
    for(const char* pcur = procpatternp; *pcur; pcur++){
        switch(*pcur){
            case 'g':{
                //GOTO GEXPTR
                u32 offset = fsmod_read_infile_ptr(fch->ptrsFp, fch->offset, *errbufpp);
                if(offset)
                    fseek(fch->ptrsFp, offset, SEEK_SET);
            } break;
            case '+':
            case '-': {
                long offset = strtol(pcur, NULL, 0);
                fseek(fch->ptrsFp, offset, SEEK_CUR);
            } break;
            case '[':{
                // LOOP START
                Stack32_push(&loopStack, (u32)(pcur - procpatternp)); // save pcur position
                Stack32_push(&loopStack, 0); // new iteration
                gexdev_u32vec_ascounter_inc(&iterVec, loopStack.sp / 2 - 1);
            } break;
            case ';':{
                // LOOP BODY END
                u32 popedIteration = Stack32_pop(&loopStack);
                u32 popedPcurPos = Stack32_pop(&loopStack);
                bool isNullTerminated = (pcur[1] == ']');
                if(isNullTerminated){
                    if(popedIteration) break;
                }
                else {
                    int repeats = atoi(pcur+1);
                    if(popedIteration >= repeats) break;
                }
                // if the loop isn't over
                pcur = procpatternp + popedPcurPos;
                Stack32_push(&loopStack, popedPcurPos);
                Stack32_push(&loopStack, popedIteration+(isNullTerminated ? 0 : 1));
                gexdev_u32vec_ascounter_inc(&iterVec, loopStack.sp / 2 - 1);
            } break;
            case 'G':{
                pcur+=1; // skip the '{'
                u32 offset = fsmod_read_infile_ptr(fch->ptrsFp, fch->offset, *errbufpp);
                if(!offset){
                    if(strFindScopeEndFromInside(pcur, '[', ';')[1] == ']'){
                        loopStack.stack[loopStack.sp-1] = 1;
                    }
                    pcur = strFindScopeEnd(pcur, '}');
                    break;
                }
                Stack32_push(&offsetStack, (u32)ftell(fch->ptrsFp) - 4);
                fseek(fch->ptrsFp, offset, SEEK_SET);
            } break;
            case '}':{
                // G scope end
                u32 offset = Stack32_pop(&offsetStack);
                fseek(fch->ptrsFp, offset+4, SEEK_SET);
            } break;
            case 'c':{
                // call callback
                cb(fch, &iterVec, internalVars, pass2cb);
                cbCalls++;
            } break;
            case 'C':{
                while(cb(fch, &iterVec, internalVars, pass2cb)) cbCalls++;
            }break;
            case 'p':{
                Stack32_push(&loopStack, (u32)ftell(fch->ptrsFp));
            } break;
            case 'b':{
                u32 offset = Stack32_pop(&loopStack);
                if(offset){
                    fseek(fch->ptrsFp, offset, SEEK_SET);
                }
            } break;
            case 'd':{
                //set data fp to gexptr offset read by ptrsFp
                u32 offset = fsmod_read_infile_ptr(fch->ptrsFp, fch->offset, *errbufpp);
                if(offset){
                    fseek(fch->dataFp, offset, SEEK_SET);
                }
            } break;
            case 'D':{
                u32 offset = fsmod_read_infile_ptr(fch->ptrsFp, fch->offset, *errbufpp);
                const char * scopeEnd = NULL;
                if(!offset){
                    if((scopeEnd = strFindScopeEndFromInside(pcur, '[', ';'))[1] == ']'){
                        loopStack.stack[loopStack.sp-1] = 1;
                        pcur = scopeEnd-1;
                    }
                    break;
                }
                fseek(fch->dataFp, offset, SEEK_SET);
            } break;
            case 'r': _fsmod_follow_pattern_read(&pcur, fch->ptrsFp, *errbufpp);
        }
    }

    Stack32_close(&offsetStack);
    Stack32_close(&loopStack);
    gexdev_u32vec_close(&iterVec);
    FSMOD_ERRBUF_REVERT(*errbufpp);
    return cbCalls;

}