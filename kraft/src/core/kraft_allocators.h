#pragma once

#include <core/kraft_core.h>

#define KRAFT_SIZE_KB(Size) Size * 1024
#define KRAFT_SIZE_MB(Size) KRAFT_SIZE_KB(Size) * 1024

namespace kraft {

struct ArenaCreateOptsT
{
    uint64 ChunkSize = 64;
    uint64 Alignment = 64;
};

// TODO: Arena->Next
struct ArenaT
{
    uint8*           Ptr;
    uint64           Capacity = 0;
    uint64           Size = 0;
    ArenaCreateOptsT Options;
};

ArenaT* CreateArena(ArenaCreateOptsT Options);
void    DestroyArena(ArenaT* Arena);
uint8*  ArenaPush(ArenaT* Arena, uint64 Size);
char*   ArenaPushString(ArenaT* Arena, const char* SrcStr, uint64 Length);

}