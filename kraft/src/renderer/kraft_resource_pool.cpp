#include "kraft_resource_pool.inl"

#include <core/kraft_string.h>
#include <core/kraft_log.h>
#include <math/kraft_math.h>
#include <renderer/vulkan/kraft_vulkan_types.h>
#include <containers/kraft_hashmap.h>

#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft::renderer {
template<typename ConcreteType, typename Type>
Pool<ConcreteType, Type>::Pool(uint16 ElementCount) : FreeListTop(0)
{
    KASSERT(ElementCount > 0);
    this->Grow(ElementCount);
}

template<typename ConcreteType, typename Type>
void Pool<ConcreteType, Type>::Grow(uint16 ElementCount)
{
    // For now we only support enlarging the pool
    KASSERT(ElementCount > this->PoolSize);

    // Allocate a new buffer and save any old buffer references
    uint64 AuxiliaryDataArraySize = sizeof(Type) * ElementCount;
    uint64 DataArraySize = sizeof(ConcreteType) * ElementCount;
    uint64 HandlesArraySize = sizeof(HandleType) * ElementCount;
    uint64 FreeListArraySize = sizeof(uint16) * ElementCount;
    uint64 NewSize = AuxiliaryDataArraySize + DataArraySize + HandlesArraySize + FreeListArraySize;
    uint8* NewBuffer = (uint8*)Malloc(NewSize, MEMORY_TAG_RESOURCE_POOL, true);

    uint64 OldAuxiliaryDataArraySize = sizeof(Type) * this->PoolSize;
    uint64 OldDataArraySize = sizeof(ConcreteType) * this->PoolSize;
    uint64 OldHandlesArraySize = sizeof(HandleType) * this->PoolSize;
    uint64 OldFreeListArraySize = sizeof(uint16) * this->PoolSize;
    uint64 OldSize = AuxiliaryDataArraySize + DataArraySize + HandlesArraySize + FreeListArraySize;

    uint8*        OldBuffer = (uint8*)this->AuxiliaryData;
    Type*         OldAuxiliaryData = (Type*)OldBuffer;
    ConcreteType* OldData = (ConcreteType*)(OldBuffer + OldAuxiliaryDataArraySize);
    HandleType*   OldHandles = (HandleType*)(OldBuffer + OldAuxiliaryDataArraySize + OldDataArraySize);
    uint16*       OldFreeList = (uint16*)(OldBuffer + OldAuxiliaryDataArraySize + OldDataArraySize + OldHandlesArraySize);

    this->AuxiliaryData = (Type*)NewBuffer;
    this->Data = (ConcreteType*)(NewBuffer + AuxiliaryDataArraySize);
    this->Handles = (HandleType*)(NewBuffer + AuxiliaryDataArraySize + DataArraySize);
    this->FreeList = (uint16*)(NewBuffer + AuxiliaryDataArraySize + DataArraySize + HandlesArraySize);

    // Now if we had any old data, copy it over
    if (this->PoolSize > 0)
    {
        MemCpy(AuxiliaryData, OldAuxiliaryData, OldAuxiliaryDataArraySize);
        MemCpy(Data, OldData, OldDataArraySize);
        MemCpy(Handles, OldHandles, OldHandlesArraySize);
        MemCpy(FreeList, OldFreeList, OldFreeListArraySize);

        Free(OldBuffer, OldSize, MEMORY_TAG_RESOURCE_POOL);
    }

    // Mark the remaining elements as free
    for (int i = this->PoolSize; i < ElementCount; i++)
    {
        this->FreeList[i] = i;
    }

    this->PoolSize = ElementCount;
}

template<typename ConcreteType, typename Type>
void Pool<ConcreteType, Type>::Delete(HandleType Resource)
{
    if (Resource.Generation != 0)
    {
        KERROR("Deleting a handle with generation > 0 | Index = %d, Generation = %d", Resource.Index, Resource.Generation);
        return;
    }

    // this->Data[Resource.Index] = {};
    // this->AuxiliaryData[Resource.Index] = {};

    KASSERT(this->FreeListTop > 0);

    this->FreeList[this->FreeListTop - 1] = Resource.Index;
    this->FreeListTop--;
}

template<typename ConcreteType, typename Type>
bool Pool<ConcreteType, Type>::ValidateHandle(HandleType Handle) const
{
    KASSERT(Handle.Index < this->PoolSize);
    if (Handle.Index < this->PoolSize)
    {
        // Data has changed?
        HandleType CurrentHandle = this->Handles[Handle.Index];
        return CurrentHandle.Generation == Handle.Generation;
    }

    return false;
}

template struct Pool<VulkanTexture, Texture>;
template struct Pool<VulkanBuffer, Buffer>;
template struct Pool<VulkanRenderPass, RenderPass>;
template struct Pool<VulkanCommandBuffer, CommandBuffer>;
template struct Pool<VulkanCommandPool, CommandPool>;

}