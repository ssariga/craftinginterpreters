#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
#include "memory.h"

int main(void) {
    // Initialize your personal heap once before allocating.
    initMemory(16u * 1024u * 1024u);

    // Create the chunk that will receive the compiler's output.
    Chunk chunk;
    initChunk(&chunk);

    // Test parsing only. Do not execute this chunk.
    bool success = compile("1 ? 2 : 3 ? 4 : 5", &chunk);

    printf("Parsing %s.\n", success ? "succeeded" : "failed");

    // Release the chunk's allocated storage.
    freeChunk(&chunk);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}