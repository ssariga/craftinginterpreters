
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"

VM vm;
//resets the stack
static void resetStack()
{
    vm.stackTop = vm.stack;
}

//intializes the VM
//creation of dynamic stack support, challenge question 3, chapter 15.
void initVM()
{
    vm.stack = NULL;
    vm.stackCapacity = 0;
    resetStack();
}

//Frees the vm dynamically.
//creation of dynamic stack support, challenge question 3, chapter 15.
void freeVM()
{
    reallocate(vm.stack,vm.stackCapacity * sizeof(Value),0);
    vm.stack = NULL;
    vm.stackCapacity = 0;
    resetStack();
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


#define BINARY_OP(op)                                              \
  do {                                                            \
    double b = pop();                                             \
    double a = pop();                                             \
    push(a op b);                                                 \
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
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            //baseline case
            case OP_NEGATE:
                push(-pop());
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
            case OP_RETURN:
            {
               Value result = pop();
            #ifndef VM_BENCHMARK
                printValue(result);
                printf("\n");
            #else
                // Preserve an observable result without printing during timing.
                static volatile Value benchmarkResult;
                benchmarkResult = result;
                (void)benchmarkResult;
            #endif
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk* chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}

