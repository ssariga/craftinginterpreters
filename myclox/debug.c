#include <stdio.h>

#include "debug.h"
#include "value.h"

/*To disassemble a chunk, we print a little header (so we can tell which chunk we’re looking at)
 *and then crank through the bytecode, disassembling each instruction.
 *The way we iterate through the code is a little odd.
 *Instead of incrementing offset in the loop, we let disassembleInstruction()
 *do it for us. When we call that function,
 *after disassembling the instruction at the given offset,
 *it returns the offset of the next instruction.  */
void disassembleChunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;)
    {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int simpleInstruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk,int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}
//OP_JUMP and OP_JUMP_IF_FALSE,
//These two instructions have a new format with a 16-bit operand,
//so we add a new utility function to disassemble them.
static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
           offset + 3 + sign * jump);
    return offset + 3;
}
//Challenge Question 2, Chapter 14.
static int longConstantInstruction(const char* name,Chunk* chunk,int offset)
{
    //The decoder reconstructs the index by shifting the middle and high bytes
    //back into position,
    //then combining them with bitwise OR. It must use the same byte
    //order as the encoder
    uint32_t constant =
        (uint32_t)chunk->code[offset + 1] |
        ((uint32_t)chunk->code[offset + 2] << 8) |
        ((uint32_t)chunk->code[offset + 3] << 16);

    printf("%-16s %4u '", name, (unsigned int)constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");

    // the disassembler must skip the opcode and all three operand bytes.
    return offset + 4;
}

int disassembleInstruction(Chunk* chunk, int offset)
{
    printf("%04d ", offset);

    // Translate the bytecode offset into its source line.
    int line = getLine(chunk, offset);

    /*it’s helpful to show which source line each instruction was compiled from.
     *That gives us a way to map back to the original code when we’re trying to figure out what
     *some blob of bytecode is supposed to do. After printing the offset
     *of the instruction—the number of bytes from the beginning of the chunk—we show its source line */
    if (offset > 0 && line == getLine(chunk, offset - 1))
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", line);
     }

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
        case OP_CONSTANT_LONG:
            return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_DUP:
            return simpleInstruction("OP_DUP", offset); //added switch support, challenge question 1, chapter 23.
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

