#pragma once

#include "stb_ds.h"

#define CreateArray(a, n) arrsetlen(a, n), kraft::MemZero(a, sizeof(*a) * n) 
#define DestroyArray(a) arrfree(a), a = 0