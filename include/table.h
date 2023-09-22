#ifndef NKCODE_TABLE_H
#define NKCODE_TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* Key;
    Value value;
} Entry;

typedef struct {
    int Count;
    int Capacity;
    Entry* Entries;
} Table;

void TableInit(Table* table);
bool TableSet(Table* table, ObjString* key, Value value);
bool TableGet(Table* table, ObjString* key, Value* value);
bool TableDelete(Table* table, ObjString* key);
void TableAddAll(Table* from, Table* to);
ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash);
void TableFree(Table* table);

#endif