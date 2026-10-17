#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include <assert.h>
#include "memory.h"
#include "vm.h"


/*REPL handles input that spans multiple lines gracefully
 *and doesn’t have a hardcoded line length limit */
static void repl()
{
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin))
        {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

//dynamically allocates the source code string
//and passes ownership to its caller
static char* readFile(const char* path)
{
    FILE* file = fopen(path, "rb");

    //If the file fails to open, run the
    //following block of code.
    if (file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);

    //if we cannot allocate enough memory to read the lox script,
    //run the following block of code.
    if (buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

    //if we are unable to read the file
    //run the following  code block.
    if (bytesRead < fileSize)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}


//reads and execute the resulting string of lox source code.
static void runFile(const char* path)
{
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[])
{

    initMemory(16u * 1024u * 1024u);
    initVM(); // initializes the VM.
    //REPL and Script loading.
    if (argc == 1)
    {
        repl();
    } else if (argc == 2)
    {
        runFile(argv[1]);
    }
    else
    {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }
    
    freeVM();
    return 0;
}