#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"

VM vm;
//resets the stack
static void resetStack()
{
    vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getLine(vm.chunk, (int)instruction);
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}
//intializes the VM
//creation of dynamic stack support, challenge question 3, chapter 15.
void initVM()
{
    vm.stack = NULL;
    vm.stackCapacity = 0;
    resetStack();
    vm.objects = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings); //intialize the table.
}

//Frees the vm dynamically.
//creation of dynamic stack support, challenge question 3, chapter 15.
void freeVM()
{
    reallocate(vm.stack,vm.stackCapacity * sizeof(Value),0);
    vm.stack = NULL;
    vm.stackCapacity = 0;
    resetStack();
    freeTable(&vm.globals);
    freeTable(&vm.strings);//free the table data.
    freeObjects();
}

//adjusted for dynamic stack support, challenge question 3, chapter 15.
void push(Value value)
{
    // Avoid subtracting NULL pointers before the first allocation.
    size_t count = 0;

    // Only subtract the pointers when an allocated stack exists.
    // Before the first allocation, both pointers are NULL.
    if (vm.stackCapacity > 0)
    {
        count = (size_t)(vm.stackTop - vm.stack);
    }

    // If every allocated slot is occupied, we need more storage.
    if (count == vm.stackCapacity)
    {
        // Save the old capacity for resizing and overflow checks.
        size_t oldCapacity = vm.stackCapacity;

        // Find the largest number of Values whose byte size fits in size_t.
        size_t maxCapacity = SIZE_MAX / sizeof(Value);

        // Hold the capacity we will request.
        size_t newCapacity;

        if (oldCapacity == 0)
        {
            // Begin with room for eight Values.
            newCapacity = 8;
        }
        else
        {
            // Check before doubling or multiplying the capacity.
            if (oldCapacity > maxCapacity / 2)
            {
                // Explain why the stack cannot be expanded.
                fprintf(stderr, "VM stack capacity limit reached.\n");
                exit(EXIT_FAILURE);
            }

            // Double capacity to avoid reallocating on every push.
            newCapacity = oldCapacity * 2;
        }

        // Check the chosen capacity, including the initial allocation case.
        if (newCapacity > maxCapacity)
        {
            // Report that the requested capacity cannot be represented safely.
            fprintf(stderr, "VM stack capacity limit reached.\n");
            exit(EXIT_FAILURE);
        }

        // Allocate or resize the array using our existing allocator.
        // The returned address may differ from the previous stack address.
        vm.stack = reallocate(vm.stack,oldCapacity * sizeof(Value),newCapacity * sizeof(Value));

        vm.stackCapacity = newCapacity;

        // The array may have moved, so reconstruct the top pointer.
        vm.stackTop = vm.stack + count;
    }

    // There is now space for at least one more value.
    *vm.stackTop = value;
    vm.stackTop++;
}

//adjusted for dynamic stack support, challenge question 3, chapter 15.
Value pop()
{
    // An empty stack has its top pointer at its starting address.
    // This also detects the initial state where both pointers are NULL.
    if (vm.stackTop == vm.stack)
    {
        fprintf(stderr, "VM stack underflow.\n");
        exit(EXIT_FAILURE);
    }

    // Move from the first unused slot back to the last occupied slot.
    vm.stackTop--;

    // Return that slot's value.
    // The slot is now logically unused because stackTop moved backward.
    return *vm.stackTop;
}

static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// Allows for the concatenation of Strings
/*
static void concatenate()
{
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
} */


static void concatenate(void)
{
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;

    char* chars = ALLOCATE(char, length + 1);

    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));

    //memcpy(result->chars,a->chars,(size_t)a->length);
    //memcpy(result->chars + a->length,b->chars, (size_t)b->length);


    //result->hash = hashString(result->chars, result->length);
    //pop();
    //pop();



    //ObjString* result = allocateString(length);

    /*
    //new memcpy calls so borrowed strings can be copied
    //Challenge Question 2, Chapter 19
    memcpy(result->data,
           a->chars,
           (size_t)a->length);

    memcpy(result->data + a->length,
           b->chars,
           (size_t)b->length);

        */

    //No longer used, due to challenge question 2, Chapter 19


}
//This is the single most important function in all of clox,
//by far. When the interpreter executes a user’s program most of its time is spent here
//It is the beating heart of the VM.
/*To process an instruction, we first figure out what kind of instruction we’re dealing with.
 *The READ_BYTE macro reads the byte currently
 *pointed at by ip and then advances the instruction pointer.
 *The first byte of any instruction is the opcode. Given a numeric opcode,
 *we need to get to the right C code that implements that instruction’s semantics.
 *This process is called decoding or dispatching the instruction. */
static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)


    /* REFINED BINARY_OP for challenge question 4, Chapter 15.
#define BINARY_OP(op) \
    do { \
    /* Avoid subtracting NULL pointers on an unallocated stack.  \
    if (vm.stackCapacity == 0 ||                                   \
        vm.stackTop - vm.stack < 2) {                             \
      fprintf(stderr, "VM stack underflow.\n");                    \
      exit(EXIT_FAILURE);                                         \
    }                                                             \
                                                                  \
    /* Preserve left/right order for subtraction and division.    \
    vm.stackTop[-2] = vm.stackTop[-2] op vm.stackTop[-1];           \
                                                                  \
    /* Two operands have become one result.                    \
    vm.stackTop--;                                                \
  } while (false) */

    for (;;)
    {
        #ifdef DEBUG_TRACE_EXECUTION

        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));

        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
            case OP_CONSTANT:
            {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value))

                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL:
            {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL:
            {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) 
                {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL:
            {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError(
                        "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            //baseline case
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            /*case OP_NEGATE:
                // Keep the underflow protection from our checked pop().
                if (vm.stackTop == vm.stack)
                {
                    fprintf(stderr, "VM stack underflow.\n");
                    exit(EXIT_FAILURE);
                }

                // stackTop points to the first unused slot.
                // The value immediately below it is the top operand.
                vm.stackTop[-1] = -vm.stackTop[-1];
                break; */
            case OP_PRINT:
            {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_RETURN:
            {
                // Exit interpreter.
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk))
    {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}

