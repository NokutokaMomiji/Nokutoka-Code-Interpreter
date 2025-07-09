#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Memory.h"
#include "Object.h"
#include "Table.h"
#include "Value.h"

#define TABLE_MAX_LOAD 0.75

void TableInit(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

static Entry* FindEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t Index = key->hash & (capacity - 1);
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
    if (table->count == 0) return false;

    Entry* entry = FindEntry(table->entries, table->capacity, key);
    if (entry->Key == NULL) return false;

    *value = entry->value;

    return true;
}

bool TableContains(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = FindEntry(table->entries, table->capacity, key);
    return (entry->Key != NULL);
}

static void AdjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].Key = NULL;
        entries[i].value = NULL_VALUE;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->Key == NULL) continue;

        Entry* dest = FindEntry(entries, capacity, entry->Key);
        dest->Key = entry->Key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool TableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        AdjustCapacity(table, capacity);
    }

    Entry* entry = FindEntry(table->entries, table->capacity, key);
    bool isNewKey = (entry->Key == NULL);
    if (isNewKey && IS_NULL(entry->value)) table->count++;

    entry->Key = key;
    entry->value = value;
    return isNewKey;
}

bool TableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = FindEntry(table->entries, table->capacity, key);
    if (entry->Key == NULL) return false;

    entry->Key = NULL;
    entry->value = BOOL_VALUE(true);
    return true;
}

void TableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->Key != NULL) {
            TableSet(to, entry->Key, entry->value);
        }
    }
}

ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0)  return NULL;

    uint32_t Index = hash & (table->capacity - 1);

    for (;;) {
        Entry* entry = &table->entries[Index];
        if (entry->Key == NULL) {
            if (IS_NULL(entry->value))  return NULL;
        }
        else if (entry->Key->length == length &&
                 entry->Key->hash == hash &&
                 memcmp(entry->Key->chars, chars, length) == 0) {
                    return entry->Key;
        }

        Index = (Index + 1) & (table->capacity - 1);
    }
}

void TableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->Key != NULL && !entry->Key->object.isMarked)
            TableDelete(table, entry->Key);
    }
}

void TableMark(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        MarkObject((Object*)entry->Key);
        MarkValue(entry->value);
    }
}

void TableFree(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    TableInit(table);
}