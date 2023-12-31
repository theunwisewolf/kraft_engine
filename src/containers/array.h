#pragma once

#include "stb_ds.h"

#include "core/kraft_memory.h"

#define CreateArray(a, n) arrsetlen(a, n), kraft::MemZero(a, sizeof(*a) * n) 
#define DestroyArray(a) arrfree(a), a = 0