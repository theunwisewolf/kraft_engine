#include "kraft_resource_manager.h"

#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <math/kraft_math.h>
#include <containers/kraft_array.h>
#include <renderer/kraft_renderer_types.h>

namespace kraft::renderer {

struct TempAllocatorStateT
{
    GPUBuffer CurrentBlock;
} TempAllocatorState;

GPUBuffer TempAllocator::Allocate(uint64 Size, uint64 Alignment)
{
    KASSERT(Size <= Allocator->BlockSize);
    uint64 Offset = kraft::math::AlignUp(CurrentOffset, Alignment);
    CurrentOffset = Offset + Size;

    if (CurrentOffset > Allocator->BlockSize)
    {
        TempAllocatorState.CurrentBlock = Allocator->GetNextFreeBlock();
        Offset = 0;
        CurrentOffset = Size;
    }

    Allocator->AllocationCount++;
    KDEBUG("Allocation Count: %d", Allocator->AllocationCount);
    return {
        .GPUBuffer = TempAllocatorState.CurrentBlock.GPUBuffer,
        .Ptr = TempAllocatorState.CurrentBlock.Ptr + Offset,
        .Offset = Offset,
    };
}

struct ResourceManagerDataT
{
    PhysicalDeviceFormatSpecs FormatSpecs;
} ResourceManagerData;

ResourceManager* ResourceManager::Ptr = nullptr;

ResourceManager::ResourceManager()
{}

ResourceManager::~ResourceManager()
{}

void ResourceManager::SetPhysicalDeviceFormatSpecs(const PhysicalDeviceFormatSpecs& Specs)
{
    ResourceManagerData.FormatSpecs = Specs;
}

const PhysicalDeviceFormatSpecs& ResourceManager::GetPhysicalDeviceFormatSpecs() const
{
    return ResourceManagerData.FormatSpecs;
}

}