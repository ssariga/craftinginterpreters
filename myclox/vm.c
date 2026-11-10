#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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
    vm.openUpvalues = NULL;
}


static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
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
        ObjFunction* function = frame->closure->function;
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

    static void defineNative(const char* name, NativeFn function)
    {
        push(OBJ_VAL(copyString(name, (int)strlen(name))));
        push(OBJ_VAL(newNative(function)));
        tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
        pop();
        pop();
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

        defineNative("clock", clockNative);
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
static bool call(ObjClosure* closure, int argCount)
{
    if (argCount != closure->function->arity)
    {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE:
            {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
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

static ObjUpvalue* captureUpvalue(Value* local)
{
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local)
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);

    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL)
    {
        vm.openUpvalues = createdUpvalue;
    }
    else
    {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last)
{
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last)
    {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
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
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])


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

        disassembleInstruction(&frame->closure->function->chunk,
       (int)(frame->ip - frame->closure->function->chunk.code));

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
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
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
            case OP_GET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
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
            case OP_JUMP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL:
            {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE:
            {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++)
                {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal)
                    {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            case OP_RETURN:
            {
                // Exit interpreter.
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0)
                {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}

