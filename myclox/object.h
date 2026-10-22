#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

//definition of the Object Type.
typedef enum {
    OBJ_STRING,
  } ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

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