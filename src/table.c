#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void TableInit(Table* table) {
    table->Count = 0;
    table->Capacity = 0;
    table->Entries = NULL;
}

static Entry* FindEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t Index = key->Hash & (capacity - 1);
    Entry* tombstone = NULL;

    for(;;) {
        Entry* entry = &entries[Index];
        if (entry->Key == NULL) {
            if (IS_NULL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            }
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->Key == key) {
            return entry;
        }

        Index = (Index + 1) & (capacity - 1);
    }
}

bool TableGet(Table* table, ObjString* key, Value* value) {
    if (table->Count == 0) return false;

    Entry* entry = FindEntry(table->Entries, table->Capacity, key);
    if (entry->Key == NULL) return false;

    *value = entry->value;
    return true;
}

static void AdjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].Key = NULL;
        entries[i].value = NULL_VALUE;
    }

    table->Count = 0;
    for (int i = 0; i < table->Capacity; i++) {
        Entry* entry = &table->Entries[i];
        if (entry->Key == NULL) continue;

        Entry* dest = FindEntry(entries, capacity, entry->Key);
        dest->Key = entry->Key;
        dest->value = entry->value;
        table->Count++;
    }

    FREE_ARRAY(Entry, table->Entries, table->Capacity);
    table->Entries = entries;
    table->Capacity = capacity;
}

bool TableSet(Table* table, ObjString* key, Value value) {
    if (table->Count + 1 > table->Capacity * TABLE_MAX_LOAD) {
        int Capacity = GROW_CAPACITY(table->Capacity);
        AdjustCapacity(table, Capacity);
    }

    Entry* entry = FindEntry(table->Entries, table->Capacity, key);
    bool isNewKey = (entry->Key == NULL);
    if (isNewKey && IS_NULL(entry->value)) table->Count++;

    entry->Key = key;
    entry->value = value;
    return isNewKey;
}

bool TableDelete(Table* table, ObjString* key) {
    if (table->Count == 0) return false;

    Entry* entry = FindEntry(table->Entries, table->Capacity, key);
    if (entry->Key == NULL) return false;

    entry->Key = NULL;
    entry->value = BOOL_VALUE(true);
    return true;
}

void TableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->Capacity; i++) {
        Entry* entry = &from->Entries[i];
        if (entry->Key != NULL) {
            TableSet(to, entry->Key, entry->value);
        }
    }
}

ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->Count == 0)  return NULL;

    uint32_t Index = hash & (table->Capacity - 1);

    for (;;) {
        Entry* entry = &table->Entries[Index];
        if (entry->Key == NULL) {
            if (IS_NULL(entry->value))  return NULL;
        }
        else if (entry->Key->Length == length &&
                 entry->Key->Hash == hash &&
                 memcmp(entry->Key->Chars, chars, length) == 0) {
                    return entry->Key;
        }

        Index = (Index + 1) & (table->Capacity - 1);
    }
}

void TableFree(Table* table) {
    FREE_ARRAY(Entry, table->Entries, table->Capacity);
    TableInit(table);
}