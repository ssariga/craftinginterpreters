#include <stdbool.h>  // Defines bool, true, and false.
#include <stddef.h>   // Defines size_t, max_align_t, and NULL.
#include <stdint.h>   // Defines SIZE_MAX, used to check for overflow.
#include <stdio.h>    // Declares fprintf() and stderr.
#include <stdlib.h>   // Declares malloc(), exit(), and EXIT_FAILURE.
#include <string.h>   // Declares memcpy().

#include "memory.h"   // Declares our public memory-management functions.

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

// Allocate, resize, or release memory using the personal heap.
// oldSize is the caller's previous requested size, not padded capacity.
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
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


static void freeObject(Obj* object)
{
  switch (object->type)
  {
    /* Original version of the OBJ_STRING case
    case OBJ_STRING:
    {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    } */
    //Challenge Question 1, Chapter 19
    //only one allocation to release. string->chars points inside that allocation,
    //so it must not be freed separately.
    //Passing the complete allocation size to your existing reallocate()
    //also preserves the allocator interface’s size information.
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
    }
  }
}

void freeObjects()
{
  Obj* object = vm.objects;
  while (object != NULL)
  {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}
