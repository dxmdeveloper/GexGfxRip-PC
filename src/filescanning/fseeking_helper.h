#pragma once
#include "filescanning.h"
#include "../essentials/vector.h"

#define INTERNAL_VAR_CNT 32

// TODO: more docs for non-recursive version
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
int fscan_follow_pattern(fscan_file_chunk fileChunk[1], const char pattern[], jmp_buf *error_jmp_buf);

/** @brief function made to speed up file seeking programming. recursive version.
  ** Internal variables:
  * function have array of 32bit variables of size INTERNAL_VAR_CNT (32)
  * @param pattern a string not bigger than 256 characters (excluding white spaces).
  ** Instructions in pattern:
  * +number/-number     - move ptrsFp by offset; 
  * g - goto gexptr     - reads and follow pointer;
  * p - push offset     - push current ptrsFp offset on stack;
  * b - back to offset  - pops offset from stack and set ptrsFp to it;
  * c - call callback   - calls callback passed as param cb. Ignores it's return value;
  * C - call callback, break on non-zero return value.
  * d - dataFp goto     - resolves gexptr ptrsFp and sets dataFp to the resolved offset;
  * D - the same as d but can break ;] loop
  * G{} - go into and back - equivalent of pg|some instructions|b+4. Can break ;] loop.
  * r(dest,n,type) - reads n values from ptrsFp to dest.
  * *  Allowed dest params: stdout, print, $(internal_var index). stdout and print is the same.
  * *  n param - count of elements to read. If internal variable is used as dest then every element will be read into next 32bit var no matter of type.
  * *  Allowed type params: X32, x32, i32, u32, X16, x16, i16, u16, X8, x8, i8, u8 - X and x are used to print number as hex.
  * *    If bit count is not given then value will be interpreted as 8bit.
  ** Loops:
  * [ instructions ;n] for loop - n is a number of repeats. Will execute at least once.
  * [ instructions ; ] null gexptr terminated do while loop. Will execute code inside until G or D breaks the loop.
  */
size_t fscan_follow_pattern_recur(fscan_file_chunk fChunk[1], const char pattern[], void *pass2cb,
				  int cb(fscan_file_chunk fChunk[1], gexdev_u32vec *iterVecp, uint32_t internalVars[INTERNAL_VAR_CNT],
					 void *clientp),
				  jmp_buf **errbufpp);

/// @brief finds end of scope by characters like } or ]
/// @param str array of character starting with opening character
const char *strFindScopeEnd(const char str[], char closeChar);

/// @brief finds end of scope by characters like } or ] from inside of scope
const char *strFindScopeEndFromInside(const char str[], char openChar, char closeChar);