#pragma once

#include <renderer/kraft_renderer_types.h>
#include <core/kraft_core.h>
#include <core/kraft_asserts.h>
#include <math/kraft_math.h>

namespace kraft
{

struct TempBuffer
{
    uint8* Ptr;
    renderer::Handle<renderer::Buffer> GPUBuffer;
    uint64 Size;
};

struct TempMemoryBlockAllocator
{
    struct Block
    {
        union
        {
            uint8* Ptr;
            renderer::Handle<renderer::Buffer> GPUBuffer; // GPU Buffer
        };
    };

    uint64 BlockSize;
    virtual Block GetNextFreeBlock() = 0;
};

struct TempAllocator
{
    uint64                           CurrentOffset;
    TempMemoryBlockAllocator::Block  CurrentBlock;
    TempMemoryBlockAllocator*        Allocator;

    uint8* Allocate(uint64 Size, uint64 Alignment)
    {
        KASSERT(Size <= Allocator->BlockSize);
        uint64 Offset = kraft::math::AlignUp(CurrentOffset, Alignment);
        CurrentOffset = Offset + Size;

        if (CurrentOffset > Allocator->BlockSize)
        {
            CurrentBlock = Allocator->GetNextFreeBlock();
            Offset = 0;
            CurrentOffset = Size;
        }

        
    }
};

}