#include <core/kraft_core.h>
#include <containers/kraft_array.h>

namespace kraft {
struct Texture;
}

namespace kraft::renderer {
template<typename T>
struct Handle;
}

namespace kraft {
template<typename T>
struct Array;
}

namespace kraft::renderer {

template<typename ConcreteType, typename Type>
struct Pool
{
    using HandleType = Handle<Type>;
    typedef void (*PoolCleanupFunction)(ConcreteType* GPUResource);

    uint16        PoolSize = 0;
    uint16        FreeListTop;
    uint64        AllocatedSizeInBytes = 0;
    Type*         AuxiliaryData;
    ConcreteType* Data;
    HandleType*   Handles;
    uint16*       FreeList;

    Array<HandleType> DeletedItems[3] = {};
    uint32            ActiveDeletedItemsArray = 0;

    Pool(uint16 ElementCount);
    ~Pool();
    void Grow(uint16 ElementCount);

    bool  ValidateHandle(HandleType Handle) const;
    Type* GetAuxiliaryData(HandleType Handle) const
    {
        return ValidateHandle(Handle) ? &this->AuxiliaryData[Handle.Index] : nullptr;
    }

    ConcreteType* Get(HandleType Handle) const
    {
        return ValidateHandle(Handle) ? &this->Data[Handle.Index] : nullptr;
    }

    HandleType Insert(Type AuxiliaryData, ConcreteType Data)
    {
        if (this->FreeListTop >= this->PoolSize)
        {
            this->Grow(this->PoolSize * 2);
        }

        uint16      Index = this->FreeList[this->FreeListTop++];
        HandleType& Handle = this->Handles[Index];
        Handle.Index = Index;
        Handle.Generation++;

        this->AuxiliaryData[Handle.Index] = AuxiliaryData;
        this->Data[Handle.Index] = Data;

        return Handle;
    }

    void* Allocate(HandleType Handle)
    {
        return (void*)(&this->Data[Handle.Index]);
    }

    uint64 GetSize() const
    {
        return this->PoolSize;
    }

    void MarkForDelete(HandleType Handle)
    {
        if (!ValidateHandle(Handle))
            return;

        uint16 Index = Handle.Index;
        this->Handles[Index].Generation = 0; // Invalidate the handle
        this->DeletedItems[this->ActiveDeletedItemsArray].Push(this->Handles[Index]);
    }

    void Delete(HandleType Resource);

    const Array<HandleType>& GetItemsToDelete()
    {
        return this->DeletedItems[(this->ActiveDeletedItemsArray + 2) % 3];
    }

    void PrepareDeleteArraysForNextFrame()
    {
        uint32 N2 = (this->ActiveDeletedItemsArray + 2) % 3;
        this->DeletedItems[N2].Clear();

        this->ActiveDeletedItemsArray = (this->ActiveDeletedItemsArray + 1) % 3;
    }

    // Removes any resources pending deletion
    void Cleanup(PoolCleanupFunction Callback)
    {
        const auto& ResourcesToDelete = this->GetItemsToDelete();
        for (int i = 0; i < ResourcesToDelete.Size(); i++)
        {
            ConcreteType* Resource = &this->Data[ResourcesToDelete[i].Index];
            Callback(Resource);

            this->Delete(ResourcesToDelete[i]);
        }

        this->PrepareDeleteArraysForNextFrame();
    }
};

}

namespace kraft {
struct Texture;
}

namespace kraft::renderer {
struct VulkanTexture;
struct VulkanBuffer;
struct VulkanRenderPass;
struct VulkanCommandBuffer;
struct VulkanCommandPool;
struct Buffer;
struct RenderPass;
struct CommandBuffer;
struct CommandPool;

extern template struct Pool<VulkanTexture, Texture>;
extern template struct Pool<VulkanBuffer, Buffer>;
extern template struct Pool<VulkanRenderPass, RenderPass>;
extern template struct Pool<VulkanCommandBuffer, CommandBuffer>;
extern template struct Pool<VulkanCommandPool, CommandPool>;

}