#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

// Temporarily count active compiler functions as roots during RC collection.
void retainCompilerRoots(void);

// Remove the temporary compiler-root counts after the RC phase.
void releaseCompilerRoots(void);
#endif