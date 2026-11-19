#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"
#include "vm.h"
#include <stddef.h> // Challenge question 3, Chapter 14


//allocates an array with a given element type and count
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)


/*a new capacity based on a given current capacity.
 *In order to get the performance we want,
 *the important part is that it scales based on the old size.
 *We grow by a factor of two, which is pretty typical.
 *1.5× is another common choice. */
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

//grow the array to the desired size
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

//free the memory
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/*Use for all dynamic memory management in clox—allocating memory,
 *freeing it, and changing the size of an existing allocation.  */
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void markObject(Obj* object);
void markValue(Value value);
void collectGarbage();
void freeObjects();
void initMemory(size_t heapSize); // Challenge question 3, Chapter 14



//Challenge Question 3, Chapter 26
// Record an additional owning reference to an object.
//Defer freeing zero-count objects rather than freeing them directly inside pop().
//Deferred reclamation lets us handle temporary references safely

void retainObject(Obj* object);
// Remove an owning reference without immediately freeing the object.
void releaseObject(Obj* object);
// Retain an object-containing Value; ignore non-object values.
void retainValue(Value value);
// Release an object-containing Value; ignore non-object values.
void releaseValue(Value value);

// Print cumulative collection totals for this process.
void printGCStats(void);
#endif