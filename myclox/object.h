#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

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

struct Obj {
    ObjType type;
    struct Obj* next;
};

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