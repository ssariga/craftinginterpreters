#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"


//Each hash table entry has this structure.
typedef struct {
    ObjString* key;
    Value value;
} Entry;

//each hash table is an array of entries.
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

//creates a new empty hash table.
void initTable(Table* table);

//free the allocation
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
#endif