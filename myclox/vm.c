#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

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
    vm.frameCount = 0;
}


//updated clockNative method
//Challenge Question 3, chapter 24.
static bool clockNative( int argCount,Value* args,Value* result, const char** error)
{
    // This native has no additional failure condition to report.
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);

    return true;
}
static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--)
    {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        int line =getLine(&function->chunk,(int)instruction);
        fprintf(stderr, "[line %d] in ", line);
        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }

    }
    resetStack();
}

//modified defineNative, for arity support, challenge question 2, chapter 24.
    static void defineNative(const char* name, NativeFn function, int arity)
    {
    // Temporarily keep the name and native object on the value stack.
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function, arity)));

    // Register the native function under its global name.
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

    // Remove the temporary values.
    pop();
    pop();
    }


//Added sqrtNative method, in order to test native functions failing.
//Challenge Question 3, Chapter 24.
static bool sqrtNative(int argCount,Value* args,Value* result, const char** error)
{
    // Check the Lox value's type before treating it as a C number.
    if (!IS_NUMBER(args[0]))
    {
        *error = "sqrt() expects a number.";
        return false;
    }

    // Extract the number only after its type has been validated.
    double number = AS_NUMBER(args[0]);

    // Reject negative inputs before calling C's sqrt().
    if (number < 0)
    {
        *error = "sqrt() expects a non-negative number.";
        return false;
    }

    // Store the successful Lox return value.
    *result = NUMBER_VAL(sqrt(number));

    // Tell the VM that the call succeeded.
    return true;
}

// Added mod function support, challenge question 4, chapter 24
static bool modNative(int argCount,Value* args,Value* result,const char** error)
{
    // Both operands must be numbers.
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
    {
        *error = "mod() expects two numbers.";
        return false;
    }

    double dividend = AS_NUMBER(args[0]);
    double divisor = AS_NUMBER(args[1]);

    // A remainder operation requires a nonzero divisor.
    if (divisor == 0)
    {
        *error = "mod() divisor cannot be zero.";
        return false;
    }

    // Preserve fractional operands instead of converting to integers.
    *result = NUMBER_VAL(fmod(dividend, divisor));
    return true;
}


//Added absolute value native function support, challenge question 4, chapter 24.
static bool absNative(int argCount,Value* args,Value* result, const char** error)
{
    // Validate the Lox argument before extracting a C number.
    if (!IS_NUMBER(args[0]))
    {
        *error = "abs() expects a number.";
        return false;
    }

    double number = AS_NUMBER(args[0]);

    // Remove a negative sign while preserving nonnegative values.
    *result = NUMBER_VAL(number < 0 ? -number : number);
    return true;
}

//added string length in bytes support for native functions, challenge question 4, chapter 24.
static bool lengthNative(int argCount,Value* args, Value* result, const char** error)
{
    // Only strings are supported by this native.
    if (!IS_STRING(args[0]))
    {
        *error = "length() expects a string.";
        return false;
    }

    ObjString* string = AS_STRING(args[0]);

    // Return the stored byte length as a Lox number.
    *result = NUMBER_VAL((double)string->length);
    return true;
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

    //added arity support, challenge question 24.
    defineNative("clock", clockNative, 0);
    defineNative("sqrt", sqrtNative, 1);

    //added additional native function support, challenge question 4, chapter 24.
    defineNative("mod", modNative, 2);
    defineNative("abs", absNative, 1);
    defineNative("length", lengthNative, 1);
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
    size_t count = 0;

    if (vm.stackCapacity > 0)
    {
        count = (size_t)(vm.stackTop - vm.stack);
    }

    if (count == vm.stackCapacity)
    {
        size_t oldCapacity = vm.stackCapacity;
        size_t maxCapacity = SIZE_MAX / sizeof(Value);
        size_t newCapacity;

        if (oldCapacity == 0)
        {
            newCapacity = 8;
        }
        else
        {
            if (oldCapacity > maxCapacity / 2)
            {
                fprintf(stderr, "VM stack capacity limit reached.\n");
                exit(EXIT_FAILURE);
            }

            newCapacity = oldCapacity * 2;
        }

        if (newCapacity > maxCapacity)
        {
            fprintf(stderr, "VM stack capacity limit reached.\n");
            exit(EXIT_FAILURE);
        }

        // Save frame positions before the allocation can move.
        size_t frameOffsets[FRAMES_MAX];

        for (int i = 0; i < vm.frameCount; i++)
        {
            frameOffsets[i] =
                (size_t)(vm.frames[i].slots - vm.stack);
        }

        vm.stack = reallocate(
            vm.stack,
            oldCapacity * sizeof(Value),
            newCapacity * sizeof(Value)
        );

        vm.stackCapacity = newCapacity;
        vm.stackTop = vm.stack + count;

        // Restore each frame's pointer into the resized stack.
        for (int i = 0; i < vm.frameCount; i++)
        {
            vm.frames[i].slots = vm.stack + frameOffsets[i];
        }
    }

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

/*simply initializes the next CallFrame on the stack.
 *It stores a pointer to the function being called and points the frame’s ip to the
 *beginning of the function’s bytecode.
 *Finally, it sets up the slots pointer to give the frame its window into the stack. */
static bool call(ObjFunction* function, int argCount)
{
    if (argCount != function->arity)
    {
        runtimeError("Expected %d arguments but got %d.",
            function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

//modified OBJ_NATIVE,
//it now prevents the reading of arguments that were never provided.
//challenge question 2, chapter 24.
//Modified OBJ_NATIVE again to be able to report a runtime error
//Challenge Question 3, Chapter 24.
static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case OBJ_FUNCTION:
                return call(AS_FUNCTION(callee), argCount);
            case OBJ_NATIVE:
            {
                // Access the object so we can read its function and expected arity.
                ObjNative* native = (ObjNative*)AS_OBJ(callee);

                // Reject an incorrect argument count before invoking the C function.
                if (argCount != native->arity)
                {
                    runtimeError("Expected %d arguments but got %d.",native->arity,argCount);

                    return false;
                }

                // Initialize outputs so failure handling never reads uninitialized data.
                Value result = NIL_VAL;
                const char* error = NULL;

                // Invoke the native and check its success flag.
                bool succeeded = native->function(argCount,vm.stackTop - argCount,&result,&error);

                if (!succeeded)
                {
                    // Use a fallback if the native did not supply a message.
                    runtimeError("%s",error != NULL ? error : "Native function failed.");

                    // runtimeError() resets the stack.
                    // Do not perform normal successful-call stack cleanup.
                    return false;
                }

                // Successful call: remove the callee and its arguments.
                vm.stackTop -= argCount + 1;

                // Replace them with the native's Lox return value.
                push(result);

                return true;
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
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

/*HEAVILY MODIFIED RUN method
*
 * Executes bytecode until the top-level script returns or an error occurs.
 *
 * Challenge Question 1, Chapter 24 optimization:
 *   - Keep the active instruction pointer in local variable 'ip'.
 *   - Use local 'ip' for instruction reads and jumps.
 *   - Save it into frame->ip before calls and runtime errors.
 *   - Reload it from the active frame after calls and returns.
 *
 * While executing ordinary instructions, local 'ip' is current.
 * The stored frame->ip may be stale until explicitly synchronized.
 */
static InterpretResult run()
{
    // Select the most recently created frame: the active function call.
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

    // Cache the active frame's instruction pointer locally.
    register uint8_t* ip = frame->ip;


    // Read the byte at ip, then advance ip to the following byte.
    // The increment moves the pointer, not the byte stored in memory.
#define READ_BYTE() (*ip++)

    /*
    * Read a two-byte, unsigned jump offset.
    *
    * First advance past both operand bytes.
    * Then combine the first byte (high eight bits) with the second.
    * The comma operator makes the combined value the expression's result.
    */
#define READ_SHORT() \
    (ip += 2, \
    (uint16_t)((ip[-2] << 8) | ip[-1]))

    // Read a one-byte constant index, then retrieve that constant
    // from the currently executing function's constant pool.
#define READ_CONSTANT() \
    (frame->function->chunk.constants.values[READ_BYTE()])

    // Read a constant that the instruction expects to contain a string.
#define READ_STRING() AS_STRING(READ_CONSTANT())

    /*
    * Synchronize the frame's stored instruction pointer before reporting.
    * runtimeError() uses stored frame pointers to identify source lines.
    *
    * __VA_ARGS__ forwards the format string and any formatting arguments.
    * do/while(false) makes this macro behave like one statement.
    *
    * This macro reports and resets the VM through runtimeError().
    * Its callers still return INTERPRET_RUNTIME_ERROR afterward.
    */
#define RUNTIME_ERROR(...) \
    do { \
        frame->ip = ip; \
        runtimeError(__VA_ARGS__); \
    } while (false)

    /*
     * Implement numeric operations shared by several instructions.
     *
     * Check both operand types without removing them.
     * Pop the right operand first, then the left operand.
     * Apply the supplied operator and wrap the result as a Lox value.
     *
     * valueType is BOOL_VAL for comparisons or NUMBER_VAL for arithmetic.
     * Operand order matters for subtraction, division, and comparisons.
     */
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            RUNTIME_ERROR("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        // Indent the stack display to distinguish it from instructions.
        printf("          ");

        // Visit every occupied slot in the VM's value stack.
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
        {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }

        // Finish the stack-display line.
        printf("\n");

        // Display the instruction that will execute next.
        // Use local ip because frame->ip may not yet be synchronized.
        // Pointer subtraction gives the byte offset within this chunk.
        disassembleInstruction(
            &frame->function->chunk,
            (int)(ip - frame->function->chunk.code)
        );
#endif

        // Hold the opcode fetched during this loop iteration.
        uint8_t instruction;

        // Fetch one opcode, advance ip, and select its implementation.
        switch (instruction = READ_BYTE())
        {
            case OP_CONSTANT:
            {
                // Read the constant-pool index and retrieve its value.
                Value constant = READ_CONSTANT();

                // Place the constant on the value stack.
                push(constant);

                // Finish this instruction and continue the outer loop.
                break;
            }

            case OP_NIL:
                // Push Lox's nil value.
                push(NIL_VAL);
                break;

            case OP_TRUE:
                // Push the Lox Boolean value true.
                push(BOOL_VAL(true));
                break;

            case OP_FALSE:
                // Push the Lox Boolean value false.
                push(BOOL_VAL(false));
                break;

            case OP_POP:
                // Remove and discard the top stack value.
                pop();
                break;

            case OP_GET_LOCAL:
            {
                // Read the local variable's slot index.
                uint8_t slot = READ_BYTE();

                // Read relative to this function call's stack window.
                // Copy the value before push() can resize the stack.
                Value value = frame->slots[slot];

                // Push a copy; leave the local variable itself unchanged.
                push(value);
                break;
            }

            case OP_SET_LOCAL:
            {
                // Read the destination local variable's slot index.
                uint8_t slot = READ_BYTE();
                // Store the top value without popping it.
                // Assignment expressions retain their assigned value.
                frame->slots[slot] = peek(0);
                break;
            }

            case OP_GET_GLOBAL:
            {
                // Read the global variable's name from the constant pool.
                ObjString* name = READ_STRING();
                // Receive the value if the globals-table lookup succeeds.
                Value value;


                // A missing table entry means the global is undefined.
                if (!tableGet(&vm.globals, name, &value))
                {
                    // Save ip and report the missing variable.
                    RUNTIME_ERROR(
                        "Undefined variable '%s'.",
                        name->chars
                    );
                    // Stop executing after the runtime error.
                    return INTERPRET_RUNTIME_ERROR;
                }

                // Make the global's value available to the expression.
                push(value);
                break;
            }

            case OP_DEFINE_GLOBAL:
            {
                // Read the name being declared.
                ObjString* name = READ_STRING();
                // Store the initializer currently on top of the stack.
                tableSet(&vm.globals, name, peek(0));

                // The declaration has consumed the initializer value.
                pop();
                break;
            }

            case OP_SET_GLOBAL:
            {
                // Read the name receiving the assignment.
                ObjString* name = READ_STRING();

                // tableSet() returns true when it creates a new entry.
                // Assignment must update an existing global, not define one.
                if (tableSet(&vm.globals, name, peek(0)))
                {
                    // Undo the insertion of the previously undefined name.
                    tableDelete(&vm.globals, name);

                    // Synchronize ip before reporting the invalid assignment.
                    RUNTIME_ERROR(
                        "Undefined variable '%s'.",
                        name->chars
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                // Leave the assigned value on the stack.
                break;
            }

            case OP_EQUAL:
            {

                // Remove the right-hand operand.
                Value b = pop();
                // Remove the left-hand operand.
                Value a = pop();
                // Compare the Lox values and push a Boolean result.
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }

            case OP_GREATER:
                // Compare numeric operands: left > right.
                BINARY_OP(BOOL_VAL, >);
                break;

            case OP_LESS:
                // Compare numeric operands: left < right.
                BINARY_OP(BOOL_VAL, <);
                break;

            case OP_ADD:
            {
                // String addition uses your existing concatenation helper.
                if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
                {
                    concatenate();
                }
                // Numeric addition requires two numeric operands.
                else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
                {
                    // Remove the right-hand number.
                    double b = AS_NUMBER(pop());

                    // Push the numeric sum.
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                }
                else
                {

                    // Reject mixed types and unsupported operand types.
                    RUNTIME_ERROR(
                        "Operands must be two numbers or two strings."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }

            case OP_SUBTRACT:
                // Subtract the right number from the left number.
                BINARY_OP(NUMBER_VAL, -);
                break;

            case OP_MULTIPLY:
                // Multiply the two numeric operands.
                BINARY_OP(NUMBER_VAL, *);
                break;

            case OP_DIVIDE:
                // Divide the left number by the right number.
                BINARY_OP(NUMBER_VAL, /);
                break;

            case OP_NOT:
                // Remove the operand, test its falseyness, and push
                // the Boolean result of logical negation.
                push(BOOL_VAL(isFalsey(pop())));
                break;

            case OP_NEGATE:
            {
                // Unary minus requires a numeric operand
                if (!IS_NUMBER(peek(0)))
                {
                    RUNTIME_ERROR("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }


                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }

            case OP_PRINT:
            {
                // Remove the expression result and print it.
                printValue(pop());
                // End the output line
                printf("\n");
                break;
            }

            case OP_JUMP:
            {
                // Read the forward distance; ip advances past the operand.
                uint16_t offset = READ_SHORT();
                // Jump forward relative to that updated position.
                ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE:
            {
                // Read the forward jump distance.
                uint16_t offset = READ_SHORT();

                if (isFalsey(peek(0)))
                {
                    // Skip forward when the condition is falsey.
                    ip += offset;
                }

                break;
            }

            case OP_LOOP:
            {
                // Read the backward jump distance.
                uint16_t offset = READ_SHORT();

                // Return to an earlier instruction in the loop.
                ip -= offset;
                break;
            }

            case OP_CALL:
            {
                // Read the number of arguments supplied to the call.
                int argCount = READ_BYTE();

                // Save the caller's position before switching frames.
                // This also supports errors reported by callValue().
                frame->ip = ip;

                // The callee lies below its arguments on the value stack.
                // callValue() handles both Lox functions and native functio
                if (!callValue(peek(argCount), argCount))
                {
                    // callValue() has already reported the failure.
                    return INTERPRET_RUNTIME_ERROR;
                }

                // Reload from the active frame after either kind of call:
                // a Lox function call or a native function call.
                frame = &vm.frames[vm.frameCount - 1];

                // Load that frame's instruction position into local ip.
                ip = frame->ip;

                break;
            }

            case OP_RETURN:
            {
                // Preserve the return value before discarding this call's stack.
                Value result = pop();


                // Remove the returning function from the active frame count.
                // The local 'frame' still points to its frame record for now.
                vm.frameCount--;

                // Returning from the script means execution is complete.
                if (vm.frameCount == 0)
                {
                    // Remove the top-level script's function value.
                    pop();

                    // Report successful completion to interpret().
                    return INTERPRET_OK;
                }

                // Discard the returning call's callee, arguments, and locals.
                // Its slot zero identifies where the caller expects the result.
                vm.stackTop = frame->slots;

                // Replace the discarded call data with its return value.
                push(result);

                // Restore the caller and its saved instruction position.
                frame = &vm.frames[vm.frameCount - 1];

                // Resume at the position saved by the caller's OP_CALL.
                ip = frame->ip;

                break;
            }
        }
    }
    // Remove these preprocessor definitions for the rest of this source file.
    // #undef directives act during preprocessing, not during VM execution.
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef RUNTIME_ERROR
#undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    call(function, 0);
    return run();
}



