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

//Core of the hash table
//responsible for taking a key and an array of buckets,
//and figuring out which bucket the entry belongs in.
//This function is also where linear probing and collision handling come into play.
static Entry* findEntry(Entry* entries, int capacity,ObjString* key)
{
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL)
        {
            if (IS_NIL(entry->value))
            {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key)
        {
            // We found the key.
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
bool tableGet(Table* table, ObjString* key, Value* value)
{
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}


//allocates an array of buckets.
static void adjustCapacity(Table* table, int capacity)
{
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }


    table->count = 0;
    //rebuild the table by scratch, by re-inserting every entry
    //into the new empty array.
    for (int i = 0; i < table->capacity; i++)
    {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++; //each time a non-tombstone entry is found, we add it.
    }

    FREE_ARRAY(Entry, table->entries, table->capacity); // release the memory for the old array.
    table->entries = entries;
    table->capacity = capacity;
}

//adds the given key pair to the given hash table.
bool tableSet(Table* table, ObjString* key, Value value)
{

    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;

    if (isNewKey && IS_NIL(entry->value))
        table->count++;

    if (isNewKey)
        table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}


//removes an entry from the table.
bool tableDelete(Table* table, ObjString* key)
{
    if (table->count == 0) return false;

    // Find the entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}
//helper function for copying all of the entries of one hash table into another.
void tableAddAll(Table* from, Table* to)
{
    for (int i = 0; i < from->capacity; i++)
    {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL)
        {
            tableSet(to, entry->key, entry->value);
        }
    }
}



ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash)
{
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
            entry->key->hash == hash &&
            memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
            }

        index = (index + 1) % table->capacity;
    }
}