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
static inline int strstartswith_ci(const char * restrict str1, const char * restrict str2){
    while(*str2){
        if(!*str1 || (toupper(*str1) != toupper(*str2))) return 0;
        str1++; str2++;
    }
    return 1;
}

/** @brief checks if str1 starts with str2. Function is case sensive.
  * @return 1 if str1 starts with str2. 0 otherwise */
static inline int strstartswith_cs(const char * restrict str1, const char * restrict str2){
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

/** @brief breaks infinite loop in next switch iteration if pcur is in such loop
  * @return EXIT_SUCCESS (break switch) or EXIT_FAILURE  */
static inline int _fscan_follow_pattern_break_infinite_loop(const char ** pcurp, Stack32* loopstackp){
    const char* loopend = NULL;
    const char* scopeend = NULL;

    loopend = strFindScopeEndFromInside(*pcurp, '[', ';');
    scopeend = strFindScopeEndFromInside(*pcurp, '{', '}');

    if(loopend && ( !scopeend || loopend < scopeend) && loopend[1] == ']'){
        // break ;] loop
        loopstackp->stack[loopstackp->sp-1] = 1;
        *pcurp = loopend - 1;
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

/// @return EXIT_SUCCESS or EXIT_FAIULRE
static inline int _fscan_follow_pattern_read(const char ** pcurp, FILE * fp, u32 *ivars, jmp_buf * error_jmp_buf){
    #define DESTTYPE_FILE 0
    #define DESTTYPE_INTERNAL_VAR 1

    unsigned long rcount = 0;
    char priformat[16] = "\0";
    uint destType = 0; // print/stdout = 0, internal var = 1
    uint ivarIndex = 0;

    if((*pcurp)[1] != '(') return EXIT_FAILURE;
    (*pcurp) += 2;
    const char * expr_end = strFindScopeEndFromInside((*pcurp), '(', ')');

    // dest param
    if(strstartswith_ci((*pcurp), "stdout")
    || strstartswith_ci((*pcurp), "print" )){
        destType = DESTTYPE_FILE;
        printf("%0lX:\n", ftell(fp));
    }
    else if(**pcurp == '$' && ivars){ 
        destType = DESTTYPE_INTERNAL_VAR;
        ivarIndex = (uint) strtoul(++(*pcurp), NULL, 0);
    }
    else {(*pcurp) = expr_end; return EXIT_FAILURE;}

    // how many values to read param
    if(!((*pcurp)=strchr((*pcurp), ','))) {(*pcurp) = expr_end; return EXIT_FAILURE;}

    rcount = strtoul(++(*pcurp), NULL, 0);

    if(destType == DESTTYPE_INTERNAL_VAR 
    && rcount >= ivarIndex + INTERNAL_VAR_CNT) {(*pcurp) = expr_end; return EXIT_FAILURE;}

    // format param
    if(!((*pcurp)=strchr((*pcurp), ','))) {(*pcurp) = expr_end; return EXIT_FAILURE;}

    uint bytes = (uint)atoi((++(*pcurp))+1) / 8;
    if(bytes==0) bytes = 1;
    bool isSigned = false;

    switch(destType){
        case DESTTYPE_FILE: {
            switch(**pcurp){
                case 'X': sprintf(priformat,"%%0%uX ", bytes*2); break;
                case 'x': sprintf(priformat,"%%0%ux ", bytes*2); break;
                case 'u': case 'U': strcpy(priformat,"%u ");     break;
                case 'i': case 'I': strcpy(priformat,"%i "); isSigned = true; break;
                default: (*pcurp) = expr_end; return EXIT_FAILURE;
            }
            if(strchr((*pcurp), 'n') && strchr((*pcurp), 'n') < expr_end)
                strcat(priformat, "\n"); 
        } break;

        case DESTTYPE_INTERNAL_VAR: {
            if(!strchr("XxUuIi", **pcurp)) {(*pcurp) = expr_end; return EXIT_FAILURE;}
            if(toupper(**pcurp) == 'I') isSigned = true;
        } break;
    }

    void * buf = calloc(rcount, bytes);
    size_t successfully_read = 0;
    switch (bytes) {
        case 1: successfully_read = fread(buf, 1, rcount, fp); break;
        case 2: successfully_read = fread_LE_U16(buf, rcount, fp); break;
        case 4: successfully_read = fread_LE_U32(buf, rcount, fp); break;
    }
    if(successfully_read < rcount){free(buf); (*pcurp) = expr_end; return EXIT_FAILURE;}

    for(unsigned long i = 0; i < rcount; i++){
        switch(destType){
            case DESTTYPE_FILE:
                switch (bytes) {
                    case 1: if(isSigned) printf(priformat, ((i8*)buf)[i]);
                            else printf(priformat, ((u8*)buf)[i]); break;
                    case 2: if(isSigned) printf(priformat, ((i16*)buf)[i]);
                            else printf(priformat, ((u16*)buf)[i]); break;
                    case 4: if(isSigned) printf(priformat, ((i32*)buf)[i]);
                            else printf(priformat, ((u32*)buf)[i]); break;
                } break;

            case DESTTYPE_INTERNAL_VAR:
                switch (bytes) {
                    case 1: if(isSigned) *(i32*)&ivars[ivarIndex + i] = (i32)((i8*)buf)[i];
                            else ivars[ivarIndex + i] = (u32)((u8*)buf)[i]; break;
                    case 2: if(isSigned) *(i32*)&ivars[ivarIndex + i] = (i32)((i16*)buf)[i];
                            else ivars[ivarIndex + i] = (u32)((u16*)buf)[i]; break;
                    case 4: if(isSigned) *(i32*)&ivars[ivarIndex + i] = ((i32*)buf)[i];
                            else ivars[ivarIndex + i] = ((u32*)buf)[i]; break;
                } break;
        }
    }
    if(destType == DESTTYPE_FILE) printf("\n");
    if(buf) free(buf);
    
    *pcurp = expr_end;
    return EXIT_SUCCESS;
    #undef DESTTYPE_FILE 
    #undef DESTTYPE_INTERNAL_VAR 
}


int fscan_follow_pattern(fscan_file_chunk * fch, const char pattern[], jmp_buf * error_jmp_buf){
    u32 u32val = 0;
    int intVal = 0;
    //u32 internalVars[INTERNAL_VAR_CNT] = {0};

    if(pattern[0] == 'e') {
        fseek(fch->ptrs_fp, fch->ep, SEEK_SET);
        pattern+=1;
    }
    for(const char* pcur = pattern; *pcur; pcur++){
        switch(*pcur){
            case 'g':{
                u32val = fscan_read_infile_ptr(fch->ptrs_fp, fch->offset, error_jmp_buf);
                if(!u32val) return EXIT_FAILURE;
                fseek(fch->ptrs_fp, u32val, SEEK_SET);
            } break;
            case '+':
            case '-': {
                intVal = atoi(pcur);
                fseek(fch->ptrs_fp, intVal, SEEK_CUR);
            } break;
            case 'd':{
                //set data fp to gexptr offset read by ptrsFp
                u32 offset = fscan_read_infile_ptr(fch->ptrs_fp, fch->offset, error_jmp_buf);
                if(!offset) return EXIT_FAILURE;
                fseek(fch->data_fp, offset, SEEK_SET);       
            } break; 
            case 'r': _fscan_follow_pattern_read(&pcur, fch->ptrs_fp, NULL, error_jmp_buf); break;
        }
    }
    return EXIT_SUCCESS;
}

// TODO: TESTS AND FIXES
size_t fscan_follow_pattern_recur(fscan_file_chunk * fch, const char pattern[], void * pass2cb,
                                  int cb(fscan_file_chunk * fch, gexdev_u32vec * iterVecp, u32 internalVars[INTERNAL_VAR_CNT], void * clientp),
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

    FSCAN_ERRBUF_CHAIN_ADD(errbufpp,
        fprintf(stderr, "err: fscan_follow_pattern_recur error");
        Stack32_close(&offsetStack);
        Stack32_close(&loopStack);
        gexdev_u32vec_close(&iterVec);
    );

    str_rewrite_without_whitespaces(procpattern, pattern, 257);

    if(pattern[0] == 'e') {
        fseek(fch->ptrs_fp, fch->ep, SEEK_SET);
        procpatternp+=1;
    }
    
    for(const char* pcur = procpatternp; *pcur; pcur++){
        switch(*pcur){
            case 'g':{
                //GOTO GEXPTR
                u32 offset = fscan_read_infile_ptr(fch->ptrs_fp, fch->offset, *errbufpp);
                if(offset)
                    fseek(fch->ptrs_fp, offset, SEEK_SET);
            } break;
            case '+':
            case '-': {
                long offset = strtol(pcur, NULL, 0);
                fseek(fch->ptrs_fp, offset, SEEK_CUR);
            } break;
            case '[':{
                // LOOP START
                Stack32_push(&loopStack, (u32)(pcur - procpatternp)); // save pcur position
                Stack32_push(&loopStack, 0); // new iteration
                if(iterVec.size < loopStack.sp / 2)
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
                    if(popedIteration + 1 >= repeats) break;
                }
                // if the loop isn't over
                pcur = procpatternp + popedPcurPos;
                Stack32_push(&loopStack, popedPcurPos);
                Stack32_push(&loopStack, popedIteration+(isNullTerminated ? 0 : 1));
                gexdev_u32vec_ascounter_inc(&iterVec, loopStack.sp / 2 - 1);
            } break;
            case 'G':{
                pcur+=1; // skip to the '{'
                u32 offset = fscan_read_infile_ptr(fch->ptrs_fp, fch->offset, *errbufpp);
                if(offset < fch->offset + fch->size / 4096 || offset >= fch->size + fch->offset){
                    if(!_fscan_follow_pattern_break_infinite_loop(&pcur, &loopStack)) break;
                    pcur = strFindScopeEnd(pcur, '}');
                    break;
                }
                Stack32_push(&offsetStack, (u32)ftell(fch->ptrs_fp) - 4);
                fseek(fch->ptrs_fp, offset, SEEK_SET);
            } break;
            case '}':{
                // G scope end
                u32 offset = Stack32_pop(&offsetStack);
                fseek(fch->ptrs_fp, offset+4, SEEK_SET);
            } break;
            case 'c':{
                // call callback
                cb(fch, &iterVec, internalVars, pass2cb);
                cbCalls++;
            } break;
            case 'C':{
                cbCalls++;
                if(!cb(fch, &iterVec, internalVars, pass2cb)){
                    _fscan_follow_pattern_break_infinite_loop(&pcur, &loopStack);
                }
            } break;
            case 'p':{
                Stack32_push(&loopStack, (u32)ftell(fch->ptrs_fp));
            } break;
            case 'b':{
                u32 offset = Stack32_pop(&loopStack);
                if(offset){
                    fseek(fch->ptrs_fp, offset, SEEK_SET);
                }
            } break;
            case 'd':{
                //set data fp to gexptr offset read by ptrsFp
                u32 offset = fscan_read_infile_ptr(fch->ptrs_fp, fch->offset, *errbufpp);
                if(offset){
                    fseek(fch->data_fp, offset, SEEK_SET);
                }
            } break;
            case 'D':{
                u32 offset = fscan_read_infile_ptr(fch->ptrs_fp, fch->offset, *errbufpp);
                if(!offset){
                    _fscan_follow_pattern_break_infinite_loop(&pcur, &loopStack);
                    break;
                }
                fseek(fch->data_fp, offset, SEEK_SET);
            } break;
            case 'r': _fscan_follow_pattern_read(&pcur, fch->ptrs_fp, internalVars, *errbufpp);
        }
    }

    Stack32_close(&offsetStack);
    Stack32_close(&loopStack);
    gexdev_u32vec_close(&iterVec);
    FSCAN_ERRBUF_REVERT(errbufpp);
    return cbCalls;

}