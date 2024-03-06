#define STB_DS_IMPLEMENTATION

#include "core/kraft_memory.h"

#define STBDS_REALLOC(context, ptr, size)   kraft::Realloc(ptr, size)
#define STBDS_FREE(context, ptr)            kraft::Free(ptr)

#include "array.h"