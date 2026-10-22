#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

/*allocates an object of the given size on the heap.
 *Note that the size is not just the size of Obj itself.
 *The caller passes in the number of bytes
 *so that there is room for the extra payload fields needed
 *by the specific object type being created.*/
static Obj* allocateObject(size_t size , ObjType type) \
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

//creates a new ObjString on the heap and then initializes its fields.
// Original version of object string.
static ObjString* allocateString(char* chars, int length, uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tableSet(&vm.strings, string, NIL_VAL);

    return string;
}


/*
//new allocate string provides flexible array support.
//Challenge Question 1, Chapter 19
ObjString* allocateString(int length)
{

    size_t size = sizeof(ObjString) + (size_t)length + 1;

    ObjString* string =
        (ObjString*)allocateObject(size, OBJ_STRING);

    string->length = length;
    string->hash = 0; // Placeholder until the characters are filled.
                        //due to challenge code implementation, I needed to adjust the architecture severely.
    tableSet(&vm.strings, string, NIL_VAL);
    string->ownsChars = true;

    string->chars = string->data;
    string->data[length] = '\0';

    //No longer used due to string borrowing, challenge Question 2, Chapter 19
    //string->chars[length] = '\0';
    return string;
}

*/

uint32_t hashString(const char* key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

//No longer used, due to flexible array implementation
//Challenge Question 1, Chapter 19
//re allowed for chapter 20 support
ObjString* takeString(char* chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length,hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocateString(chars, length, hash);
}

//Original version of the copyString.
ObjString* copyString(const char* chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString* interned =
        tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL)
    {
        return interned;
    }

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

/*
//provides flexible array support, challenge Question 1, Chapter 19.
ObjString* copyString(const char* chars, int length)
{
    ObjString* string = allocateString(length);

    // memcpy(string->chars, chars, (size_t)length);
    //new memcpy call for string borrowing
    //Challenge Question 2, Chapter 19
    memcpy(string->data, chars, (size_t)length);
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length,hash);

    if (interned != NULL)
        return interned;

    string -> hash = hashString(string -> chars, length);

    return string;
}

*/

//definition of borrowString method

/*
//A borrowed string allocates only the object header.
//It neither copies nor modifies the supplied characters.
//Challenge Question 2, Chapter 19
ObjString* borrowString(const char* chars, int length)
{
    ObjString* string =
        (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);

    string->hash = hashString(chars, length);
    string->length = length;
    string->ownsChars = false;
    string->chars = chars;

    return string;
} */

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        /*
        //new case OBJ_STRING
        //is length aware
        //challenge question 2, Chapter 19
        case OBJ_STRING: {
            ObjString* string = AS_STRING(value);

            fwrite(string->chars,
                   sizeof(char),
                   (size_t)string->length,
                   stdout);

            break;
        } */


    }
}


/*
//Used to replace
/*ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

A result of Chapter 19's challenge implementation.*/
/*
ObjString* internString(ObjString* string) {
    string->hash = hashString(string->chars, string->length);

    ObjString* interned =
        tableFindString(&vm.strings,string->chars,string->length, string->hash);

    if (interned != NULL) {
        // This newly created candidate must still be the newest object.
        assert(vm.objects == (Obj*)string);

        // Remove it from the VM's object list before freeing it.
        vm.objects = string->obj.next;

        size_t size = sizeof(ObjString);

        if (string->ownsChars) {
            size += (size_t)string->length + 1;
        }

        reallocate(string, size, 0);

        return interned;
    }

    tableSet(&vm.strings, string, NIL_VAL);

    return string;
}
*/