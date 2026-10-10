#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

/*Initailizes the chunk */
void initChunk(Chunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;

    chunk->lineCount = 0;  //challenge question 1, chapter 14
    chunk->lineCapacity = 0; //challenge question 1, chapter 14
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line)
{
    // Grow the bytecode array when necessary.
    if (chunk->capacity < chunk->count + 1)
    {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);

        chunk->code = GROW_ARRAY(uint8_t,chunk->code,oldCapacity, chunk->capacity);
    }

    // Append the bytecode byte.
    chunk->code[chunk->count] = byte;
    chunk->count++;

    // Extend the previous run if this byte has the same source line.
    if (chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line)
    {
        chunk->lines[chunk->lineCount - 1].count++;
        return;
    }

    // A different source line requires a new run.
    if (chunk->lineCapacity < chunk->lineCount + 1)
    {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);

        chunk->lines = GROW_ARRAY(
            LineRun,
            chunk->lines,
            oldCapacity,
            chunk->lineCapacity);
    }

    chunk->lines[chunk->lineCount].line = line;
    chunk->lines[chunk->lineCount].count = 1;
    chunk->lineCount++;
}

// Added support for  compressing the line information for a series of instructions on the same line
// challenge question 1, chapter 14
int getLine(const Chunk* chunk, int offset)
{
    // Reject offsets outside the bytecode array.
    if (offset < 0 || offset >= chunk->count)
    {
        return -1;
    }

    // Subtract complete runs until the offset falls inside one.
    for (int i = 0; i < chunk->lineCount; i++)
    {
        if (offset < chunk->lines[i].count)
        {
            return chunk->lines[i].line;
        }

        offset -= chunk->lines[i].count;
    }

    // Valid offsets should always be covered by a run.
    return -1;
}

int addConstant(Chunk* chunk, Value value)
{
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

//Challenge Question 2, Chapter 14.
void writeConstant(Chunk* chunk, Value value, int line)
{
    // The next constant's index must fit into 24 bits.
    if (chunk->constants.count > 0xFFFFFF)
    {
        fprintf(stderr, "Too many constants in one chunk.\n");
        exit(EXIT_FAILURE);
    }

    int index = addConstant(chunk, value);

    if (index <= 0xFF)
    {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, (uint8_t)index, line);
    }
    else
    {
        writeChunk(chunk, OP_CONSTANT_LONG, line);

        // Write the 24-bit index in little-endian order.
        writeChunk(chunk, (uint8_t)(index & 0xFF), line);
        writeChunk(chunk, (uint8_t)((index >> 8) & 0xFF), line);
        writeChunk(chunk, (uint8_t)((index >> 16) & 0xFF), line);
    }
}