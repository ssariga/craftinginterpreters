#include <stdbool.h>  // Defines bool, true, and false.
#include <stddef.h>   // Defines size_t, max_align_t, and NULL.
#include <stdint.h>   // Defines SIZE_MAX, used to check for overflow.
#include <stdio.h>    // Declares fprintf() and stderr.
#include <stdlib.h>   // Declares malloc(), exit(), and EXIT_FAILURE.
#include <string.h>   // Declares memcpy().

#include "compiler.h"
#include "memory.h"   // Declares our public memory-management functions.



// Collection totals for this process; shutdown frees are not included.
//Challenge Question 3, Chapter 26.
static size_t gcCollectionCount = 0;
static size_t gcReferenceCountedObjects = 0;
static size_t gcTracedObjects = 0;


//everything starting from here is for challenge question 3, chapter 14
// Declare the Block type so its definition can contain Block pointers.
typedef union Block Block;

// Each block starts with this header, followed immediately by its payload.
// The union's alignment member gives the header suitable alignment.
union Block
{
  struct
  {
    size_t size;  // Payload capacity; does not include the header.
    bool free;    // true means the payload is available for allocation.
    Block* prev;  // Previous physical block in the heap, or NULL.
    Block* next;  // Next physical block in the heap, or NULL.
  } info;

  // This member is not used to store data.
  // It ensures the union has alignment suitable for ordinary C types.
  max_align_t alignment;
};

// Points to the first block in our heap.
// NULL means the heap has not been initialized.
static Block* heapHead = NULL;

// Report an unrecoverable allocator error and terminate the process.
static void memoryError(const char* message)
{
  // Print the supplied message to the standard error stream.
  fprintf(stderr, "%s\n", message);

  // Stop execution with a failure status.
  exit(EXIT_FAILURE);
}

// Round a requested payload size up to the required alignment.
static size_t alignSize(size_t size)
{
  // Get the alignment requirement used for block headers and payloads.
  size_t alignment = _Alignof(max_align_t);

  // Determine how far size is past the previous aligned boundary.
  size_t remainder = size % alignment;

  // A zero remainder means size is already correctly aligned.
  if (remainder == 0)
  {
    // No padding is necessary.
    return size;
  }

  // Calculate the bytes needed to reach the next aligned boundary.
  size_t padding = alignment - remainder;

  // Check whether adding padding would overflow size_t.
  if (size > SIZE_MAX - padding)
  {
    // Refuse an allocation whose rounded size cannot be represented.
    memoryError("Allocation size overflow.");
  }

  // Return the requested size plus its alignment padding.
  return size + padding;
}

// Obtain the one large region that backs our personal heap.
void initMemory(size_t heapSize)
{
  // Prevent a second initialization from losing the original region.
  if (heapHead != NULL)
  {
    // This implementation permits initialization only once.
    memoryError("Personal heap already initialized.");
  }

  // Determine the minimum aligned payload size.
  size_t alignment = _Alignof(max_align_t);

  // Require enough room for a header and at least one aligned payload unit.
  if (heapSize < sizeof(Block) + alignment)
  {
    // A smaller region cannot hold a useful initial block.
    memoryError("Personal heap is too small.");
  }

  // Make our only call to the system allocator.
  // All subsequent allocations come from this region.
  void* region = malloc(heapSize);

  // Check whether the system allocation succeeded.
  if (region == NULL)
  {
    // We cannot run this allocator without its backing region.
    memoryError("Could not initialize personal heap.");
  }

  // Place the first block header at the beginning of the region.
  heapHead = region;

  // Everything after that first header is potential payload space.
  size_t capacity = heapSize - sizeof(Block);

  // Round down so the initial payload capacity is alignment-compatible.
  // Any leftover bytes at the end of the region remain unused.
  capacity -= capacity % alignment;

  // Initially, one block covers all usable payload space.
  heapHead->info.size = capacity;

  // None of the initial payload has been allocated yet.
  heapHead->info.free = true;

  // The first block has no predecessor.
  heapHead->info.prev = NULL;

  // There are no other blocks until we split this one.
  heapHead->info.next = NULL;
}

// Absorb the block immediately following block.
// Precondition: block->info.next exists and is free.
static void mergeNext(Block* block)
{
  // Save the neighboring block that will be absorbed.
  Block* next = block->info.next;

  // Reclaim both the neighbor's header and its payload.
  // Its header becomes part of the expanded payload capacity.
  block->info.size += sizeof(Block) + next->info.size;

  // Remove the absorbed block from the linked list.
  block->info.next = next->info.next;

  // If another block follows the absorbed block, repair its backward link.
  if (block->info.next != NULL) {
    // Its predecessor is now block, not the absorbed neighbor.
    block->info.next->info.prev = block;
  }
}

// Keep wanted bytes in block and turn usable excess into a free block.
// Preconditions: wanted is aligned and does not exceed block's capacity.
static void splitBlock(Block* block, size_t wanted) {
  // Calculate the capacity left after reserving the requested payload.
  size_t remaining = block->info.size - wanted;

  // Determine the smallest useful aligned payload for a new block.
  size_t alignment = _Alignof(max_align_t);

  // A remainder needs space for both its header and some payload.
  if (remaining < sizeof(Block) + alignment) {
    // Leave the small excess inside the original block instead.
    return;
  }

  // block + 1 points immediately after the current header.
  // Casting to unsigned char* makes pointer arithmetic count bytes.
  // Adding wanted locates the new header after the retained payload.
  Block* remainder =
      (Block*)((unsigned char*)(block + 1) + wanted);

  // The remainder's payload excludes its newly created header.
  remainder->info.size = remaining - sizeof(Block);

  // The new remainder is available for future allocations.
  remainder->info.free = true;

  // The retained block immediately precedes the remainder.
  remainder->info.prev = block;

  // The remainder takes the original block's next neighbor.
  remainder->info.next = block->info.next;

  // If that neighbor exists, update its backward link.
  if (remainder->info.next != NULL) {
    // The new remainder now physically precedes this neighbor.
    remainder->info.next->info.prev = remainder;
  }

  // Reduce the original block's recorded payload capacity.
  block->info.size = wanted;

  // Insert the remainder immediately after the original block.
  block->info.next = remainder;

  // Shrinking an allocation can create a remainder beside free space.
  // Merge such neighbors so we do not leave adjacent free blocks.
  while (remainder->info.next != NULL &&
         remainder->info.next->info.free) {
    // Absorb one free neighbor, then check again.
    mergeNext(remainder);
  }
}

// Find and occupy the first free block large enough for wanted bytes.
// Precondition: wanted is nonzero and already aligned.
static void* allocateBlock(size_t wanted) {
  // Start at the heap's first block and follow the physical block list.
  for (Block* block = heapHead;
       block != NULL;
       block = block->info.next) {
    // Reject occupied blocks and free blocks that are too small.
    if (!block->info.free || block->info.size < wanted) {
      // Try the next block.
      continue;
    }

    // Return useful excess capacity to the free pool, when possible.
    splitBlock(block, wanted);

    // Reserve this block for the caller.
    block->info.free = false;

    // Skip the header and return the beginning of the payload.
    return block + 1;
  }

  // No individual free block can satisfy the request.
  memoryError("Personal heap exhausted or fragmented.");

  // memoryError() exits; this return is never reached.
  return NULL;
}

// Return a previously allocated block to the personal heap.
// Precondition: block is a valid header for a live allocation.
static void releaseBlock(Block* block) {
  // Make the block available for reuse.
  block->info.free = true;

  // Join it with any immediately following free blocks.
  while (block->info.next != NULL &&
         block->info.next->info.free) {
    // Recover the next block's header and payload capacity.
    mergeNext(block);
  }

  // Check whether the preceding physical block is also free.
  if (block->info.prev != NULL &&
      block->info.prev->info.free) {
    // Use the previous block as the beginning of the merged region.
    block = block->info.prev;

    // Absorb the block we just released and any following free blocks.
    while (block->info.next != NULL &&
           block->info.next->info.free) {
      // Merge one neighbor per iteration.
      mergeNext(block);
    }
  }
}

#include "vm.h"
#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

// Allocate, resize, or release memory using the personal heap.
// oldSize is the caller's previous requested size, not padded capacity.
void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{

  vm.bytesAllocated += newSize - oldSize;
  //the GC (Garbage Collector) runs as often as it possibly can.
  //Poor performance, but great for flushing out memory management bugs
  if (newSize > oldSize)
  {
    #ifdef DEBUG_STRESS_GC
    collectGarbage();
    #endif


    if (vm.bytesAllocated > vm.nextGC)
    {
      collectGarbage();
    }
  }

  // A zero-size request means release the allocation.
  if (newSize == 0) {
    // There is nothing to release when pointer is NULL.
    if (pointer != NULL) {
      // Move backward by one header to recover the block metadata.
      Block* block = (Block*)pointer - 1;

      // Mark the block free and coalesce adjacent free space.
      releaseBlock(block);
    }

    // A released allocation has no payload pointer.
    return NULL;
  }

  // Nonzero allocation requests require an initialized backing region.
  if (heapHead == NULL) {
    // Catch a missing initMemory() call.
    memoryError("Call initMemory() before allocating.");
  }

  // Round the new payload request up to the required alignment.
  size_t wanted = alignSize(newSize);

  // A NULL pointer means create a new allocation.
  if (pointer == NULL) {
    // Find a free block and return its payload.
    return allocateBlock(wanted);
  }

  // Recover the header belonging to the existing payload.
  Block* block = (Block*)pointer - 1;

  // The allocation may already have enough capacity.
  // This includes shrinking and growth within existing spare capacity.
  if (block->info.size >= wanted) {
    // Release excess capacity if it can form a useful free block.
    splitBlock(block, wanted);

    // The payload did not move.
    return pointer;
  }

  // Try to grow without moving by absorbing following free blocks.
  while (block->info.size < wanted &&
         block->info.next != NULL &&
         block->info.next->info.free) {
    // Increase this allocation's capacity by reclaiming its neighbor.
    mergeNext(block);
  }

  // Check whether the newly expanded block satisfies the request.
  if (block->info.size >= wanted) {
    // Split off any useful excess beyond the requested capacity.
    splitBlock(block, wanted);

    // In-place growth preserves the original pointer.
    return pointer;
  }

  // In-place growth failed, so allocate a separate larger block.
  void* replacement = allocateBlock(wanted);

  // Preserve only the bytes common to the old and new requested sizes.
  size_t copySize = oldSize < newSize ? oldSize : newSize;

  // Copy the old contents into the new allocation.
  memcpy(replacement, pointer, copySize);

  // The old allocation is no longer needed.
  releaseBlock(block);

  // Return the new payload address.
  return replacement;
}

//This is the end of the implementation
//for challenge question 3, chapter 14



//Challenge Question 3, Chapter 26 method.
void retainObject(Obj* object)
{
  // A null pointer does not own or reference an object.
  if (object == NULL) return;

  // Prevent wrapping the counter back to zero.
  if (object->referenceCount == SIZE_MAX)
  {
    // Report an ownership-count overflow.
    fprintf(stderr, "Reference count overflow.\n");

    // Stop rather than continue with an invalid count.
    exit(EXIT_FAILURE);
  }

  // Record one additional owning reference.
  object->referenceCount++;
}


//Challenge Question 3, Chapter 26 method.
void releaseObject(Obj* object)
{
  // Releasing a null pointer requires no work.
  if (object == NULL)
    return;

  // A zero count cannot lose another recorded reference.
  if (object->referenceCount == 0)
  {
    // Expose an ownership-accounting error instead of wrapping.
    fprintf(stderr, "Reference count underflow.\n");

    // Stop so we can diagnose the incorrect ownership transfer.
    exit(EXIT_FAILURE);
  }

  // Remove one recorded owning reference.
  object->referenceCount--;

}



//Challenge Question 3, Chapter 26 method.
void retainValue(Value value)
{
  // Numbers, Booleans, and nil do not reference heap objects.
  if (!IS_OBJ(value)) return;

  // Retain the object stored inside this Value.
  retainObject(AS_OBJ(value));
}


//Challenge Question 3, Chapter 26 method.
void releaseValue(Value value)
{
  // Non-object values require no ownership accounting.
  if (!IS_OBJ(value)) return;

  // Release the object stored inside this Value.
  releaseObject(AS_OBJ(value));
}



void markObject(Obj* object)
{
  if (object == NULL)
    return;

  // Do not enqueue an object that has already been reached.
  if (isMarked(object))
    return;

  #ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
  #endif

  // Set the packed mark flag without changing the other header fields.
  setIsMarked(object, true);


  if (vm.grayCapacity < vm.grayCount + 1)
  {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack = (Obj**)realloc(vm.grayStack,sizeof(Obj*) * vm.grayCapacity);

    if (vm.grayStack == NULL) exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value)
{

  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

//Helper method for blackenObject
//every function has a constant table packed full of references to other objects.
//they are traced using this method.
static void markArray(ValueArray* array)
{
  for (int i = 0; i < array->count; i++)
  {
    markValue(array->values[i]);
  }
}

//implements blackening, indication that mark phase is done processing the object.
//traverse a single object’s references
static void blackenObject(Obj* object)
{

  #ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
  #endif

  switch (objType(object))
  {
    case OBJ_CLOSURE:
    {
      ObjClosure* closure = (ObjClosure*)object;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++)
      {
        markObject((Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION:
    {
      ObjFunction* function = (ObjFunction*)object;
      markObject((Obj*)function->name);
      markArray(&function->chunk.constants);
      break;
    }
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue*)object)->closed);
      break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
  }
}


//Challenge Question 3, Chapter 26.
// Report collection activity without including final shutdown cleanup.
void printGCStats(void)
{
  fprintf(stderr,
          "\nGC statistics:\n"
          "  Collections: %zu\n"
          "  Objects reclaimed by reference counting: %zu\n"
          "  Objects reclaimed by tracing: %zu\n",
          gcCollectionCount,
          gcReferenceCountedObjects,
          gcTracedObjects);
}


static void freeObject(Obj* object)
{

  //executed at the end of an objects lifespan.
  #ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, (int)objType(object));
  #endif
  switch (objType(object))
  {
    case OBJ_CLOSURE:
    {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION:
    {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    //Original version of the OBJ_STRING case
    case OBJ_STRING:
    {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }

    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;

    //Challenge Question 1, Chapter 19
    //only one allocation to release. string->chars points inside that allocation,
    //so it must not be freed separately.
    //Passing the complete allocation size to your existing reallocate()
    //also preserves the allocator interface’s size information.
    //commented out to make Chapter 20 work.
    /*
    case OBJ_STRING:
    {

      //Neither kind frees chars separately:
      //Owned characters are part of the object allocation.
      //Borrowed characters belong to someone else.
      //The ownership flag determines the allocation size passed back to your allocator
      ObjString* string = (ObjString*)object;

      size_t size = sizeof(ObjString);
      if (string->ownsChars)
      {
        sizeof(ObjString) + (size_t)string->length + 1;
      }


      reallocate(object, size, 0);
      break;
    } */
  }
}

//walk the local variables or temporaries sitting on the VM stack.
static void markRoots()
{
  markValue(vm.pendingPush);
  
  for (Value* slot = vm.stack; slot != vm.stackTop; slot++)
  {
    markValue(*slot);
  }

  //VM uses pointers to access constants and upvalues,
  //So they must be accessed as well.
  for (int i = 0; i < vm.frameCount; i++)
  {
    markObject((Obj*)vm.frames[i].closure);
  }


  //open upvalue list is another set of values that the VM can directly reach.
  for (ObjUpvalue* upvalue = vm.openUpvalues;upvalue != NULL; upvalue = upvalue->next)
  {
    markObject((Obj*)upvalue);
  }

    markTable(&vm.globals);
    markCompilerRoots();
}

//we keep pulling out gray objects, traversing their references,
//and then marking them black. Traversing an object’s references
//may turn up new white objects
//that get marked gray and added to the stack.
//So this function swings back and forth between
//turning white objects gray and gray objects black,
//gradually advancing the entire wavefront forward.
static void traceReferences()
{
  while (vm.grayCount > 0)
  {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}


// Remove counted outgoing references without freeing any object storage.
// Call only while all referenced objects are still allocated.
//Challenge Question 3, Chapter 26
static void detachObjectReferences(Obj* object)
{
  switch (objType(object))
  {
    case OBJ_CLOSURE:
    {
      ObjClosure* closure = (ObjClosure*)object;

      // Drop the closure's reference to its function.
      releaseObject((Obj*)closure->function);
      closure->function = NULL;

      // Drop every populated upvalue reference.
      for (int i = 0; i < closure->upvalueCount; i++)
      {
        releaseObject((Obj*)closure->upvalues[i]);
        closure->upvalues[i] = NULL;
      }

      break;
    }

    case OBJ_FUNCTION:
    {
      ObjFunction* function = (ObjFunction*)object;
      ValueArray* constants = &function->chunk.constants;

      // Drop references owned by the constant array.
      for (int i = 0; i < constants->count; i++)
      {
        releaseValue(constants->values[i]);
        constants->values[i] = NIL_VAL;
      }

      // Drop the function's owning reference to its name.
      releaseObject((Obj*)function->name);
      function->name = NULL;
      break;
    }

    case OBJ_UPVALUE:
    {
      ObjUpvalue* upvalue = (ObjUpvalue*)object;

      // Only a closed upvalue owns its captured value.
      if (upvalue->location == &upvalue->closed)
      {
        releaseValue(upvalue->closed);
        upvalue->closed = NIL_VAL;
      }

      break;
    }

    case OBJ_NATIVE:
    case OBJ_STRING:
      // Neither object type owns references to other managed objects.
      break;
  }
}




// Reclaim objects with no counted incoming references.
// VM and compiler roots must be temporarily retained before calling.
//Challenge Question 3, Chapter 26.
static void collectZeroCountObjects(void)
{
  bool reclaimed;

  do
  {
    reclaimed = false;

    Obj* previous = NULL;
    Obj* object = vm.objects;

    while (object != NULL)
    {
      if (object->referenceCount != 0)
      {
        previous = object;
        object = objNext(object);
        continue;
      }

      // Preserve the object and its successor before unlinking.
      Obj* unreached = object;
      object = objNext(unreached);

      // Remove the object from the allocation list.
      if (previous != NULL)
      {
        setObjNext(previous, object);
      }
      else
      {
        vm.objects = object;
      }

      // The intern table is weak and must not retain a stale key.
      if (objType(unreached) == OBJ_STRING)
      {
        tableDelete(&vm.strings, (ObjString*)unreached);
      }

      // Dropping outgoing references may expose more zero-count objects.
      detachObjectReferences(unreached);

      freeObject(unreached);

      // Count objects reclaimed by the zero-reference phase.
      gcReferenceCountedObjects++;

      reclaimed = true;
    }

    // Repeat because a decrement may have affected an object
    // that this traversal had already passed.
  }
  while (reclaimed);
}

//Reclamation of garbage.
//outer while loop walks the linked list of every object in the heap,
//checking their mark bits. If an object is marked (black),
//we leave it alone and continue past it. If it is unmarked (white),
//we unlink it from the list and free it
//using the freeObject() function we already wrote.
//Modified sweep method to handle the modified handling of the compact values.
//Challenge Question 1, Chapter 26.
static void sweep()
{

  // First detach every dead object's counted outgoing references.
  // No objects are freed during this pass.
  for (Obj* candidate = vm.objects;
       candidate != NULL;
       candidate = objNext(candidate))
  {
    if (!isMarked(candidate))
    {
      detachObjectReferences(candidate);
    }
  }

  // The existing pass below now unlinks and frees dead objects.
  // No surviving object precedes the head of the list.
  Obj* previous = NULL;

  // Start at the first allocated object.
  Obj* object = vm.objects;

  // Walk the complete allocation list.
  while (object != NULL)
  {
    // Reachable objects survive this collection.
    if (isMarked(object))
    {
      // Prepare the survivor for the next collection.
      setIsMarked(object, false);

      // Remember the last surviving object.
      previous = object;

      // Follow the decoded allocation-list link.
      object = objNext(object);
    }
    else
    {
      // Save the unreachable object so we can free it.
      Obj* unreached = object;

      // Read its successor before freeing its storage.
      object = objNext(object);

      // An interior removal must repair the predecessor's link.
      if (previous != NULL)
      {
        // Bypass the unreachable object.
        setObjNext(previous, object);
      }
      else
      {
        // A head removal changes the VM's list head.
        vm.objects = object;
      }

      // Release the unlinked object.
      freeObject(unreached);
      // Count objects reclaimed by the tracing backup.
      gcTracedObjects++;
    }
  }
}

//Modified collectGarbage for reference counter support.
//Challenge Question 3, Chapter 26
void collectGarbage()
{

  // Count one complete RC-plus-tracing collection.
  gcCollectionCount++;

  #ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
  #endif

  // Temporarily include roots that are not permanently reference-counted.
  retainVMRoots();
  retainCompilerRoots();

  // Reclaim zero-count objects and any resulting zero-count cascade.
  collectZeroCountObjects();

  // Return counters to their normal heap-reference-only state.
  releaseCompilerRoots();
  releaseVMRoots();

  // Keep tracing as the backup for cycles and other unreachable objects.
  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();


  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
  #ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytesAllocated, before, vm.bytesAllocated,
        vm.nextGC);
  #endif

}


void freeObjects()
{

  // At shutdown, every remaining object will be destroyed.
  // Detach references while all their targets are still allocated.
  for (Obj* candidate = vm.objects; candidate != NULL; candidate = objNext(candidate))
  {
    detachObjectReferences(candidate);
  }

  Obj* object = vm.objects;

  while (object != NULL)
  {
    // Preserve the successor before freeing the current object.
    Obj* next = objNext(object);
    freeObject(object);
    object = next;
  }

  // The allocation list is now empty.
  vm.objects = NULL;
  vm.grayStack = NULL;
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  free(vm.grayStack);
}
