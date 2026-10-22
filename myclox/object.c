#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

/*allocates an object of the given size on the heap.
 *Note that the size is not just the size of Obj itself.
 *The caller passes in the number of bytes
 *so that there is room for the extra payload fields needed
 *by the specific object type being created.*/
static Obj* allocateObject(size_t size, ObjType type)
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

//creates a new ObjString on the heap and then initializes its fields.
/* Original version of object string.
static ObjString* allocateString(char* chars, int length)
{


    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;


} */

//new allocate string provides flexible array support.
//Challenge Question 1, Chapter 19
ObjString* allocateString(int length)
{
    size_t size = sizeof(ObjString) + (size_t)length + 1;

    ObjString* string =
        (ObjString*)allocateObject(size, OBJ_STRING);

    string->length = length;
    string->ownsChars = true;
    string->chars = string->data;
    string->data[length] = '\0';

    //No longer used due to string borrowing, challenge Question 2, Chapter 19
    //string->chars[length] = '\0';

    return string;
}


/*No longer used, due to flexible array implementation
 * Challenge Question 1, Chapter 19
ObjString* takeString(char* chars, int length)
{
    return allocateString(chars, length);
} */

/* Original version of the copyString.
ObjString* copyString(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
} */

//provides flexible array support, challenge Question 1, Chapter 19.
ObjString* copyString(const char* chars, int length)
{
    ObjString* string = allocateString(length);

    // memcpy(string->chars, chars, (size_t)length);

    //new memcpy call for string borrowing
    //Challenge Question 2, Chapter 19
    memcpy(string->data, chars, (size_t)length);
    return string;
}


//definition of borrowString method

//A borrowed string allocates only the object header.
//It neither copies nor modifies the supplied characters.
//Challenge Question 2, Chapter 19
ObjString* borrowString(const char* chars, int length)
{
    ObjString* string =
        (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);

    string->length = length;
    string->ownsChars = false;
    string->chars = chars;

    return string;
}
void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
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
        }
        /*
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break; */
    }
}