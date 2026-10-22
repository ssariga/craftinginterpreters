#ifndef clox_vm_h
#define clox_vm_h
#define STACK_MAX 256

#include "chunk.h"
#include "value.h"

typedef struct
{
    Chunk* chunk;
    uint8_t* ip;
    //Creation of dynamic stack support
    //challenge question 3, chapter 15
    Value* stack;
    Value* stackTop;
    size_t stackCapacity;

    Obj* objects;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
}   InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();


#endif