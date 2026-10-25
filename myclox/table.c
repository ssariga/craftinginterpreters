#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#define TABLE_MAX_LOAD 0.75

void initTable(Table* table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}


void freeTable(Table* table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

//generic key matching support .
//Challenge Question 1, Chapter 20.
static uint32_t hashNumber(double number) {
    // +0 and -0 must produce the same hash.
    if (number == 0.0) {
        return 0;
    }

    // Use one hash for every NaN representation.
    if (number != number)
    {
        return 0x7fc00000u;
    }

    _Static_assert(sizeof(double) == sizeof(uint64_t), "This hash requires a 64-bit double.");

    uint64_t bits;
    memcpy(&bits, &number, sizeof(bits));

    bits ^= bits >> 33;
    bits *= UINT64_C(0xff51afd7ed558ccd);
    bits ^= bits >> 33;
    bits *= UINT64_C(0xc4ceb9fe1a85ec53);
    bits ^= bits >> 33;

    return (uint32_t)(bits ^ (bits >> 32));
}

//generic key matching support .
//Challenge Question 1, Chapter 20.
static uint32_t hashValue(Value key)
{
    switch (key.type) {
        case VAL_BOOL:
            return AS_BOOL(key) ? 1u : 2u;

        case VAL_NIL:
            return 3u;

        case VAL_NUMBER:
            return hashNumber(AS_NUMBER(key));

        case VAL_OBJ:
            // Chapter 20's only object type is a string.
            return AS_STRING(key)->hash;
    }

    return 0;
}


/*Ordinary numeric equality makes NaN unequal to itself.
 *If a table uses that rule unchanged, inserting a NaN key and then looking it up will fail.
 *A dictionary needs an explicit policy: reject NaN keys or define a special key-equality rule.
For this implementation, we treat all NaNs as the same table key:
*/
static bool keysEqual(Value a, Value b)
{
    if (IS_NUMBER(a) && IS_NUMBER(b))
    {
        double x = AS_NUMBER(a);
        double y = AS_NUMBER(b);

        if (x != x && y != y)
        {
            return true;
        }
    }

    return valuesEqual(a, b);
}
//Core of the hash table
//responsible for taking a key and an array of buckets,
//and figuring out which bucket the entry belongs in.
//This function is also where linear probing and collision handling come into play.
//Modified to  preserves the book’s linear probing strategy
//while replacing string-pointer comparisons with generic key matching.
//Challenge Question 1, Chapter 20.
static Entry* findEntry(Entry* entries, int capacity, Value key)
{
    uint32_t index = hashValue(key) % capacity;
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->state == ENTRY_EMPTY)
        {
            // Empty entry.
            return tombstone != NULL ? tombstone : entry;
        }
        if (entry->state == ENTRY_TOMBSTONE)
        {
            if (tombstone == NULL)
            {
                tombstone = entry;
            }
        }
        else if (keysEqual(entry->key, key))
        {
            //entry found
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

//If the table is completely empty, we definitely won’t find the entry,
//so we check for that first
//we let findEntry() work its magic. That returns a pointer to a bucket.
//If the bucket is empty, which we detect by seeing if the key is NULL,
//then we didn’t find an Entry with our key.
//If findEntry() does return a non-empty Entry, then that’s our match.
//We take the Entry’s value and copy it to the output parameter so the caller can get it.
//generic key matching support .
//Challenge Question 1, Chapter 20.
bool tableGet(Table* table, Value key, Value* value)
{
    if (table->count == 0)
    {
        return false;
    }

    Entry* entry =
        findEntry(table->entries, table->capacity, key);

    if (entry->state != ENTRY_OCCUPIED)
    {
        return false;
    }

    *value = entry->value;
    return true;
}


//allocates an array of buckets.
static void adjustCapacity(Table* table, int capacity)
{
    Entry* entries = ALLOCATE(Entry, capacity);
    // Initialize every bucket as genuinely empty.
    for (int i = 0; i < capacity; i++)
    {
        entries[i].key = NIL_VAL;
        entries[i].value = NIL_VAL;
        entries[i].state = ENTRY_EMPTY;
    }


    table->count = 0;
    //rebuild the table by scratch, by re-inserting every entry
    //into the new empty array.
    for (int i = 0; i < table->capacity; i++)
    {
        Entry* entry = &table->entries[i];

        if (entry->state != ENTRY_OCCUPIED)
        {
            continue;
        }

        Entry* dest =
            findEntry(entries, capacity, entry->key);

        dest->key = entry->key;
        dest->value = entry->value;
        dest->state = ENTRY_OCCUPIED;

        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity); // release the memory for the old array.
    table->entries = entries;
    table->capacity = capacity;
}

//adds the given key pair to the given hash table.
//generic key matching support .
//Challenge Question 1, Chapter 20.
bool tableSet(Table* table, Value key, Value value)
{

    if (table->count + 1 >
       table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
       }

    Entry* entry =
        findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->state != ENTRY_OCCUPIED;

    // A tombstone is already included in count.
    // Only a genuinely empty bucket increases count.
    if (entry->state == ENTRY_EMPTY) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    entry->state = ENTRY_OCCUPIED;

    return isNewKey;
}


//removes an entry from the table.
//generic key matching support .
//Challenge Question 1, Chapter 20.
bool tableDelete(Table* table, Value key)
{
    if (table->count == 0) {
        return false;
    }

    Entry* entry =
        findEntry(table->entries, table->capacity, key);

    if (entry->state != ENTRY_OCCUPIED)
    {
        return false;
    }

    entry->key = NIL_VAL;
    entry->value = NIL_VAL;
    entry->state = ENTRY_TOMBSTONE;

    return true;
}
//helper function for copying all of the entries of one hash table into another.
//generic key matching support .
//Challenge Question 1, Chapter 20.
void tableAddAll(Table* from, Table* to)
{
    if (from == to)
    {
        return;
    }

    for (int i = 0; i < from->capacity; i++)
    {
        Entry* entry = &from->entries[i];

        if (entry->state == ENTRY_OCCUPIED)
        {
            tableSet(to, entry->key, entry->value);
        }
    }
}



ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash)
{
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash % table->capacity;

    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->state == ENTRY_EMPTY) {
            return NULL;
        }

        if (entry->state == ENTRY_OCCUPIED &&
            IS_STRING(entry->key)) {
            ObjString* string = AS_STRING(entry->key);

            if (string->length == length &&
                string->hash == hash &&
                memcmp(string->chars, chars, length) == 0) {
                return string;
                }
            }

        index = (index + 1) % table->capacity;
    }
}