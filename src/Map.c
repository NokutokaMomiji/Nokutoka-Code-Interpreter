#include <stdio.h>
#include "Map.h"

bool MapGet(ObjMap* map, Value key, Value* value) {

}

bool MapSet(ObjMap* map, Value key, Value value) {
    if (!IS_OBJECT(key))
        return false;

    if (!IS_STRING(key))
        return false;

    ObjString* Key = AS_STRING(key);
    ValueArrayWrite(&map->keys, OBJECT_VALUE(Key));
    TableSet(&map->items, Key, value);
    return true;
}