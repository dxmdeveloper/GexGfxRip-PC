#pragma once
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../essentials/Vector.h"

//TODO: DOCS
//TODO: DOCS
//TODO: DOCS
//TODO: DOCS
//TODO: DOCS
/**
 * @brief Do not use with user input
 * 
 * @param pattern 
 * e at beggining - go to entry point
 * g - get and follow gexptr; 
 * d - get address from gexptr using ptrsFp and goto with dataFp
 * +(integer) or -(integer) - move file position by offset   
 * @return int EXIT_SUCCESS or EXIT_FAILURE.
 */
int fsmod_follow_pattern(fsmod_file_chunk fileChunk[1], const char pattern[], jmp_buf * error_jmp_buf);

// [g;5]
// g - goto gexptr
// p - push offset
// b - back (pop offset)
// c - call callback
// d - get address from gexptr using ptrsFp and goto with dataFp
// D - the same as d but can break ;] loop
// B - b+4
// G{} - pg{}B
// ;z]
size_t fsmod_follow_pattern_recur(fsmod_file_chunk fChunk[1], const char pattern[], void * pass2cb,
                                  void cb(fsmod_file_chunk fChunk[1], gexdev_u32vec * iterVecp, void * clientp), jmp_buf * error_jmp_buf);

/// @brief finds end of scope by characters like } or ]
/// @param str array of character starting with opening character
const char * strFindScopeEnd(const char str[], char closeChar);

/// @brief finds end of scope by characters like } or ] from inside of scope
const char * strFindScopeEndFromInside(const char str[], char openChar, char closeChar);