#pragma once

#define CreateArray(a, n) arrsetlen(a, n), kraft::MemZero(a, sizeof(*a) * n)
#define DestroyArray(a)   arrfree(a), a = 0
