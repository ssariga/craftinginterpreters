#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

//definition of the Object Type.
typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
  } ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

//Declaration of Function object struct.
typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;


/*
 *Challenge Question 3, Chapter 24.
 * Returns true on success and writes the Lox return value to *result.
 * Returns false on failure and writes an error message to *error.
 *
 * The error message must remain valid after the native returns.
 * String literals satisfy that requirement.
 */
typedef bool (*NativeFn)
(
    int argCount,
    Value* args,
    Value* result,
    const char** error
);

typedef struct
{
    Obj obj;
    NativeFn function;
    int arity;  // Number of arguments this native function expects. Challenge Question 2, chapter 24.
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


ObjFunction* newFunction();
ObjNative* newNative(NativeFn function, int arity); // modified newNative, for arity support, challenge question 2, chapter 24.

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
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

/*
//declaration of borrowString method,
//Challenge Question 2, Chapter 19
ObjString* borrowString(const char* chars, int length);
*/

//uint32_t hashString(const char* chars, int length);


//ObjString* internString(ObjString* string);

#endif