#ifndef clox_object_h
#define clox_object_h

#include <stdint.h>  // Provides uint64_t, uintptr_t, and UINT64_C.
#include <stdio.h>   // Provides fprintf() and stderr.
#include <stdlib.h>  // Provides exit() and EXIT_FAILURE.
#include <stddef.h>  // Provides size_t for reference counts. For hybrid GCcollection, Challenge Question 3, Chapter 26

#include "common.h"
#include "chunk.h"
#include "value.h"
// Read the object type from the packed header.
#define OBJ_TYPE(value)        (objType(AS_OBJ(value)))

#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

//definition of the Object Type.
typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
  } ObjType;

// Replace the original three fields with one packed integer.
//Placing the three variables, type, isMarker, and next into a single header.
//Challenge Question 1, Chapter 26.
struct Obj
{
    // Bits 0–47: next pointer; bit 48: mark; bits 56–63: type.
    uint64_t header;

    // Number of owning references; maintained by the reference-counting collector.
    size_t referenceCount;
};

// Select the low 48 bits containing the next-object address.
#define OBJ_NEXT_MASK UINT64_C(0x0000ffffffffffff)

// Select the single bit containing the garbage collector's mark.
#define OBJ_MARK_MASK UINT64_C(0x0001000000000000)

// Select the highest byte containing the object type.
#define OBJ_TYPE_MASK UINT64_C(0xff00000000000000)

// Encode a pointer only if this layout can represent its address.
static inline uint64_t objPointerBits(Obj* pointer)
{
    // Represent the end of the allocation list with zero bits.
    if (pointer == NULL) return UINT64_C(0);

    // Convert the address to an unsigned integer capable of holding it.
    uintptr_t bits = (uintptr_t)pointer;

    // Reject any address using bits outside our 48-bit pointer field.
    if ((bits & ~(uintptr_t)OBJ_NEXT_MASK) != 0)
    {
        // Report the unsupported address layout.
        fprintf(stderr, "Object pointer does not fit packed header.\n");

        // Stop instead of silently truncating the pointer.
        exit(EXIT_FAILURE);
    }

    // Return the checked address in the header's integer type.
    return (uint64_t)bits;
}

// Decode the object type.
static inline ObjType objType(const Obj* object)
{
    // Isolate the highest byte and shift it down to an enum value.
    return (ObjType)((object->header & OBJ_TYPE_MASK) >> 56);
}

// Read the mark flag.
static inline bool isMarked(const Obj* object)
{
    // A set mark bit produces true; a cleared bit produces false.
    return (object->header & OBJ_MARK_MASK) != 0;
}

// Decode the next pointer in the allocation list.
static inline Obj* objNext(const Obj* object)
{
    // Remove metadata and retain only the address bits.
    uintptr_t bits = (uintptr_t)(object->header & OBJ_NEXT_MASK);

    // Decode an all-zero pointer field as the end of the list.
    if (bits == 0) return NULL;

    // Convert the stored address back to an object pointer.
    return (Obj*)bits;
}

// Update the mark while preserving every other header bit.
static inline void setIsMarked(Obj* object, bool marked)
{
    // Clear the existing mark bit.
    object->header &= ~OBJ_MARK_MASK;

    // Set it again only when the caller requests a marked object.
    if (marked) object->header |= OBJ_MARK_MASK;
}

// Update the allocation-list link while preserving the metadata.
static inline void setObjNext(Obj* object, Obj* next)
{
    // Validate and encode the replacement pointer first.
    uint64_t bits = objPointerBits(next);

    // Clear the old address bits and insert the replacement address.
    object->header = (object->header & ~OBJ_NEXT_MASK) | bits;
}

// Initialize all fields without reading uninitialized header memory.
static inline void initObjHeader(Obj* object, ObjType type, Obj* next)
{
    // Enforce the challenge's limit of eight types numbered 0 through 7.
    if ((unsigned int)type > 7u)
    {
        // Explain why this object cannot be encoded.
        fprintf(stderr, "Object type exceeds packed-header design.\n");

        // Stop instead of creating an invalid header.
        exit(EXIT_FAILURE);
    }

    // Store the type and address; omitted bits, including the mark, are zero.
    object->header =
        ((uint64_t)type << 56) | objPointerBits(next);
}

//Declaration of Function object struct.
typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;


typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

/*
//payload for strings definition.
struct ObjString {
  Obj obj;
  int length;
  char chars[]; //flexible array support, challenge Question 1, Chapter 19

}; */


//New payload for strings, provides greater memory efficiency
//challenge Question 2, Chapter 19 (formerly, now remade to standard version.
struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
    //bool ownsChars;

    //char data[];
};

typedef struct ObjUpvalue
{
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct
{
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;


ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);

/*
* Before: two allocations
[ Obj | length | chars pointer ] ──→ [ characters | '\0' ]
After: one allocation
[ Obj | length | characters | '\0' ]
 *
 */


//ObjString* allocateString(int length);
//contains the characters for string concatenation.

//no longer using takeString due to the updated concatenation
//Challenge Question 1, Chapter 19.

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

//Modified for Challenge Question 1, Chapter 26.
static inline bool isObjType(Value value, ObjType type)
{
    // Verify that the value is an object before decoding its type.
    return IS_OBJ(value) && objType(AS_OBJ(value)) == type;
}

/*
//declaration of borrowString method,
//Challenge Question 2, Chapter 19
ObjString* borrowString(const char* chars, int length);
*/

//uint32_t hashString(const char* chars, int length);


//ObjString* internString(ObjString* string);

#endif