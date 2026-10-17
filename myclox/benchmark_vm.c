#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

int main(void)
{
    initMemory(16u * 1024u * 1024u);
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    // Add two constants once; their indices are 0 and 1.
    int zero = addConstant(&chunk, NUMBER_VAL(0));
    int one = addConstant(&chunk, NUMBER_VAL(1));

    // Begin with a running total of zero.
    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, (uint8_t)zero, 1);

    /* done for OP_NEGATE Challenge Question 4, Chapter 15. */
    writeConstant(&chunk, NUMBER_VAL(1), 1);
    for (int i = 0; i < 100000; i++)
    {
        writeChunk(&chunk, OP_NEGATE, 1);
    }
    writeChunk(&chunk, OP_RETURN, 1);

    /* done for BINARY_OP Challenge Question 4, Chapter 15.
    // Generate repeated additions without growing the stack indefinitely.
    for (int i = 0; i < 100000; i++)
    {
        writeChunk(&chunk, OP_CONSTANT, 1);
        writeChunk(&chunk, (uint8_t)one, 1);
        writeChunk(&chunk, OP_ADD, 1);
    }

    writeChunk(&chunk, OP_RETURN, 1);
    */

    // Warm up the VM and allocate any needed stack storage.
    if (interpret(&chunk) != INTERPRET_OK)
    {
        return EXIT_FAILURE;
    }

    // Run several trials using the same already-built chunk.
    for (int trial = 0; trial < 5; trial++)
    {
        clock_t start = clock();

        if (start == (clock_t)-1)
        {
            fprintf(stderr, "Could not read CPU time.\n");
            return EXIT_FAILURE;
        }

        // Each trial executes ten million additions.
        for (int repetition = 0; repetition < 100; repetition++) {
            if (interpret(&chunk) != INTERPRET_OK) {
                return EXIT_FAILURE;
            }
        }

        clock_t end = clock();

        if (end == (clock_t)-1) {
            fprintf(stderr, "Could not read CPU time.\n");
            return EXIT_FAILURE;
        }

        double seconds = (double)(end - start) / CLOCKS_PER_SEC;

        printf("Trial %d: %.6f seconds\n", trial + 1, seconds);
    }

    freeChunk(&chunk);
    freeVM();

    return EXIT_SUCCESS;
}