#ifndef MOMIJI_MAP_H
#define MOMIJI_MAP_H

#include "Object.h"
#include "Value.h"

bool MapGet(ObjMap* map, Value key, Value* value);
bool MapSet(ObjMap* map, Value key, Value value);

#endif