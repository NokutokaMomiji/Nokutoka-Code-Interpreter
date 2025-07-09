#ifndef MOMIJI_TABLE_H
#define MOMIJI_TABLE_H

#include "Common.h"
#include "Value.h"

typedef struct {
    ObjString* Key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void TableInit(Table* table);
bool TableSet(Table* table, ObjString* key, Value value);
bool TableGet(Table* table, ObjString* key, Value* value);
bool TableContains(Table* table, ObjString* key);
bool TableDelete(Table* table, ObjString* key);
void TableAddAll(Table* from, Table* to);
ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash);
void TableRemoveWhite(Table* table);
void TableMark(Table* table);
void TableFree(Table* table);

#endif