#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

/*each instruction has a one-byte operation code (universally shortened to opcode).
 *That number controls what kind of
 *instruction we’re dealing with—add, subtract, look up variable, etc. */
typedef enum
{
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_CONSTANT_LONG, //for challenge question 2, chapter 14.
    OP_NEGATE,
    OP_RETURN,
} OpCode;


// One run represents consecutive bytecode bytes from the same source line.
//compresses the line run, challenge question 1, chapter 14
typedef struct
{
    int line;
    int count;
} LineRun;

/*Store some other data along with the instructions*/

typedef struct
{
    int count;
    int capacity;
    uint8_t* code;
    int lineCount; //challenge question 1, chapter 14
    int lineCapacity; // challenge question 1, chapter 14
    LineRun* lines; // challenge question 1, chapter 14
    ValueArray constants;
}   Chunk;

//intializes a new chunk
void initChunk(Chunk* chunk);

//frees chunks for use.
void freeChunk(Chunk* chunk);

//appends a byte after a chunk
void writeChunk(Chunk* chunk, uint8_t byte, int line);

//Our should write to the constant array inside Chunk directly
int addConstant(Chunk* chunk, Value value);

//getLine method declaration
int getLine(const Chunk* chunk, int offset);

// chooses the instruction based on the constant’s index
void writeConstant(Chunk* chunk, Value value, int line);
#endif