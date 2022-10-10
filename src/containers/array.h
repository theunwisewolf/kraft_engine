#pragma once

#include "stb_ds.h"

namespace kraft
{

#define CreateArray(a, n) arrsetlen(a, n), MemZero(a, sizeof(*a) * n) 
#define DestroyArray(a) arrfree(a), a = 0

}