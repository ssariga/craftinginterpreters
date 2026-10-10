#include "common.h"
#include "chunk.h"
#include "debug.h"
#include <assert.h>
#include "memory.h"
#include "vm.h"

int main(int argc, const char* argv[])
{
    //intializes the vm
    initVM();
    Chunk chunk;
    initChunk(&chunk);

    //Challenge Question 3, Chapter 14
    initMemory(16u * 1024u * 1024u);

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    constant = addConstant(&chunk, 3.4);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    writeChunk(&chunk, OP_ADD, 123);
    constant = addConstant(&chunk, 5.6);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    writeChunk(&chunk, OP_DIVIDE, 123);
    writeChunk(&chunk, OP_NEGATE, 123);
    writeChunk(&chunk, OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM(); //free the vm for use
    freeChunk(&chunk);
    return 0;
}