#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "memory.h"

//Challenge question 3, chapter 14
// Exercise allocation, freeing, in-place resizing, and moving resizing.
// Call this only after initMemory() has initialized the personal heap.
static void testPersonalHeap(void)
{
  // Allocate the first block, whose contents we will preserve.
  unsigned char* first = reallocate(NULL, 0, 64);

  // Allocate an adjacent block that we can later free.
  unsigned char* second = reallocate(NULL, 0, 64);

  // Keep another allocation after second to bound the free region.
  unsigned char* guard = reallocate(NULL, 0, 64);

  // Store a distinct value in each byte of the first allocation.
  for (int i = 0; i < 64; i++) {
    // Values 0 through 63 fit into unsigned char.
    first[i] = (unsigned char)i;
  }

  // Release the block immediately after first.
  // Passing newSize == 0 requests deallocation.
  reallocate(second, 64, 0);

  // Grow first using the newly available adjacent space.
  unsigned char* grown = reallocate(first, 64, 128);

  // Verify that growth happened without moving the payload.
  assert(grown == first);

  // Check that the original 64 bytes survived the resize.
  for (int i = 0; i < 64; i++)
  {
    // Every original byte should retain its initial value.
    assert(grown[i] == (unsigned char)i);
  }

  // Reduce the requested size from 128 bytes to 32 bytes.
  unsigned char* shrunk = reallocate(grown, 128, 32);

  // Shrinking should retain the same payload address.
  assert(shrunk == grown);

  // Only the first 32 bytes remain part of the requested allocation.
  for (int i = 0; i < 32; i++)
  {
    // Verify that the retained contents are unchanged.
    assert(shrunk[i] == (unsigned char)i);
  }

  // Release the shrunken allocation and verify the NULL result.
  assert(reallocate(shrunk, 32, 0) == NULL);

  // Release the guard; adjacent free regions can now coalesce.
  assert(reallocate(guard, 64, 0) == NULL);

  // Releasing a NULL pointer should do nothing and return NULL.
  assert(reallocate(NULL, 0, 0) == NULL);

  // Allocate another block whose growth we will force to move.
  unsigned char* moving = reallocate(NULL, 0, 64);

  // Occupy the following block so moving cannot absorb it.
  unsigned char* blocker = reallocate(NULL, 0, 64);

  // Fill the original payload with a recognizable byte pattern.
  memset(moving, 0xA5, 64);

  // Request more capacity than moving currently has.
  // The occupied blocker prevents this allocator from growing in place.
  unsigned char* moved = reallocate(moving, 64, 1024);

  // Verify that the replacement occupies a different address.
  assert(moved != moving);

  // Check that moving the allocation preserved its original contents.
  for (int i = 0; i < 64; i++) {
    // Only the original 64 bytes have defined test values.
    assert(moved[i] == 0xA5);
  }

  // Release the larger replacement allocation.
  reallocate(moved, 1024, 0);

  // Release the block that prevented in-place growth.
  reallocate(blocker, 64, 0);
}
//Challenge question 2, chapter 14
static void testConstantEncoding(void)
{
    Chunk chunk;
    initChunk(&chunk);

    // Populate indices 0 through 254 without emitting instructions.
    for (int i = 0; i < 255; i++)
    {
        addConstant(&chunk, (Value)i);
    }

    // Index 255: largest index that fits in the short instruction.
    writeConstant(&chunk, (Value)255, 10);

    // Index 256: first index that needs the long instruction.
    writeConstant(&chunk, (Value)256, 20);

    writeChunk(&chunk, OP_RETURN, 30);

    assert(chunk.constants.count == 257);
    assert(chunk.count == 7);

    // Short instruction occupies offsets 0 and 1.
    assert(chunk.code[0] == OP_CONSTANT);
    assert(chunk.code[1] == 0xFF);

    // Long instruction occupies offsets 2 through 5.
    assert(chunk.code[2] == OP_CONSTANT_LONG);
    assert(chunk.code[3] == 0x00);
    assert(chunk.code[4] == 0x01);
    assert(chunk.code[5] == 0x00);

    assert(chunk.code[6] == OP_RETURN);

    // Verify line information for opcodes and operands.
    assert(getLine(&chunk, 0) == 10);
    assert(getLine(&chunk, 1) == 10);

    for (int offset = 2; offset <= 5; offset++)
    {
        assert(getLine(&chunk, offset) == 20);
    }

    assert(getLine(&chunk, 6) == 30);

    // Verify the disassembler advances by the correct instruction size.
    assert(disassembleInstruction(&chunk, 0) == 2);
    assert(disassembleInstruction(&chunk, 2) == 6);
    assert(disassembleInstruction(&chunk, 6) == 7);

    freeChunk(&chunk);
}

// Test portion for challenge question 1, chapter 14, uncomment to make it work.
int main(void)
{
    // Initialize the heap first.
    initMemory(16u * 1024u * 1024u);

    // Call the allocator test here.
    testPersonalHeap();

    Chunk chunk;
    initChunk(&chunk);

    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 123);

    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 124);

    writeChunk(&chunk, OP_RETURN, 125);

    // Six bytecode bytes should produce three line runs.
    assert(chunk.count == 6);
    assert(chunk.lineCount == 3);

    assert(chunk.lines[0].line == 123);
    assert(chunk.lines[0].count == 3);

    assert(chunk.lines[1].line == 124);
    assert(chunk.lines[1].count == 2);

    assert(chunk.lines[2].line == 125);
    assert(chunk.lines[2].count == 1);

    // Check both sides of each run boundary.
    assert(getLine(&chunk, 0) == 123);
    assert(getLine(&chunk, 2) == 123);
    assert(getLine(&chunk, 3) == 124);
    assert(getLine(&chunk, 4) == 124);
    assert(getLine(&chunk, 5) == 125);

    // Invalid offsets return the documented sentinel.
    assert(getLine(&chunk, -1) == -1);
    assert(getLine(&chunk, 6) == -1);

    disassembleChunk(&chunk, "compressed lines");

    freeChunk(&chunk);

    //test implementation of constant encoding, challenge question 2, chapter 14.s
    testConstantEncoding();
    puts("All chunk tests passed.");
    return 0;
}
