#include "fseeking_helper.h"

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
        }
    }
    return EXIT_SUCCESS;
}

// TODO: TESTS AND FIXES
size_t fsmod_follow_pattern_recur(fsmod_file_chunk * fch, const char pattern[], void * pass2cb,
                                     int cb(fsmod_file_chunk * fch, gexdev_u32vec * iterVecp, void * clientp), jmp_buf ** errbufpp){
    Stack32 offsetStack = {0};
    Stack32 loopStack = {0};
    gexdev_u32vec iterVec = {0};
    size_t cbCalls = 0;

    gexdev_u32vec_init_capcity(&iterVec, 4);
    Stack32_init(&offsetStack, 128);
    Stack32_init(&loopStack, 256);

    FSMOD_ERRBUF_EXTEND(*errbufpp,
        Stack32_close(&offsetStack);
        Stack32_close(&loopStack);
        gexdev_u32vec_close(&iterVec);
    );

    if(pattern[0] == 'e') {
        fseek(fch->ptrsFp, fch->ep, SEEK_SET);
        pattern+=1;
    }
    
    for(const char* pcur = pattern; *pcur; pcur++){
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
                Stack32_push(&loopStack, (u32)(pcur - pattern)); // save pcur position
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
                pcur = pattern + popedPcurPos;
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
                cb(fch, &iterVec, pass2cb);
                cbCalls++;
            } break;
            case 'C':{
                while(cb(fch, &iterVec, pass2cb)) cbCalls++;
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
        }
    }

    Stack32_close(&offsetStack);
    Stack32_close(&loopStack);
    gexdev_u32vec_close(&iterVec);
    FSMOD_ERRBUF_REVERT(*errbufpp);
    return cbCalls;

}