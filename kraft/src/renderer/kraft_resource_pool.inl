#pragma once

namespace kraft {
struct Texture;
struct TextureSampler;
} // namespace kraft

namespace kraft::r {
template<typename T>
struct Handle;
}

namespace kraft {
template<typename T>
struct Array;
}

namespace kraft::r {

template<typename ConcreteType, typename Type>
struct Pool
{
    using HandleType = Handle<Type>;
    typedef void (*PoolCleanupFunction)(ConcreteType* GPUResource);

    u16           PoolSize = 0;
    u16           FreeListTop;
    u64           AllocatedSizeInBytes = 0;
    Type*         AuxiliaryData;
    ConcreteType* Data;
    HandleType*   Handles;
    u16*          FreeList;

    Array<HandleType> DeletedItems[3] = {};
    u32               ActiveDeletedItemsArray = 0;

    void Grow(u16 ElementCount)
    {
        // For now we only support enlarging the pool
        KASSERT(ElementCount > this->PoolSize);

        // Allocate a new buffer and save any old buffer references
        u64 AuxiliaryDataArraySize = sizeof(Type) * ElementCount;
        u64 DataArraySize = sizeof(ConcreteType) * ElementCount;
        u64 HandlesArraySize = sizeof(HandleType) * ElementCount;
        u64 FreeListArraySize = sizeof(u16) * ElementCount;
        u64 NewSize = AuxiliaryDataArraySize + DataArraySize + HandlesArraySize + FreeListArraySize;
        u8* NewBuffer = (u8*)Malloc(NewSize, MEMORY_TAG_RESOURCE_POOL, true);

        u64 OldAuxiliaryDataArraySize = sizeof(Type) * this->PoolSize;
        u64 OldDataArraySize = sizeof(ConcreteType) * this->PoolSize;
        u64 OldHandlesArraySize = sizeof(HandleType) * this->PoolSize;
        u64 OldFreeListArraySize = sizeof(u16) * this->PoolSize;
        u64 OldSize = this->AllocatedSizeInBytes;
        this->AllocatedSizeInBytes = NewSize;

        u8*           OldBuffer = (u8*)this->AuxiliaryData;
        Type*         OldAuxiliaryData = (Type*)OldBuffer;
        ConcreteType* OldData = (ConcreteType*)(OldBuffer + OldAuxiliaryDataArraySize);
        HandleType*   OldHandles = (HandleType*)(OldBuffer + OldAuxiliaryDataArraySize + OldDataArraySize);
        u16*          OldFreeList = (u16*)(OldBuffer + OldAuxiliaryDataArraySize + OldDataArraySize + OldHandlesArraySize);

        this->AuxiliaryData = (Type*)NewBuffer;
        this->Data = (ConcreteType*)(NewBuffer + AuxiliaryDataArraySize);
        this->Handles = (HandleType*)(NewBuffer + AuxiliaryDataArraySize + DataArraySize);
        this->FreeList = (u16*)(NewBuffer + AuxiliaryDataArraySize + DataArraySize + HandlesArraySize);

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

    void Destroy()
    {
        u64 AuxiliaryDataArraySize = sizeof(Type) * this->PoolSize;
        u64 DataArraySize = sizeof(ConcreteType) * this->PoolSize;
        u64 HandlesArraySize = sizeof(HandleType) * this->PoolSize;
        u64 FreeListArraySize = sizeof(u16) * this->PoolSize;
        u64 Size = AuxiliaryDataArraySize + DataArraySize + HandlesArraySize + FreeListArraySize;

        Free(this->AuxiliaryData, Size, MEMORY_TAG_RESOURCE_POOL);
    }

    bool ValidateHandle(HandleType Handle) const
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

    Type* GetAuxiliaryData(HandleType handle) const
    {
        return ValidateHandle(handle) ? &this->AuxiliaryData[handle.Index] : nullptr;
    }

    ConcreteType* Get(HandleType handle) const
    {
        return ValidateHandle(handle) ? &this->Data[handle.Index] : nullptr;
    }

    HandleType Insert(Type AuxiliaryData, ConcreteType Data)
    {
        if (this->FreeListTop >= this->PoolSize)
        {
            this->Grow(this->PoolSize * 2);
        }

        u16         Index = this->FreeList[this->FreeListTop++];
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

    u64 GetSize() const
    {
        return this->PoolSize;
    }

    void MarkForDelete(HandleType Handle)
    {
        if (!ValidateHandle(Handle))
            return;

        u16 Index = Handle.Index;
        this->Handles[Index].Generation = 0; // Invalidate the handle
        this->DeletedItems[this->ActiveDeletedItemsArray].Push(this->Handles[Index]);
    }

    void Delete(HandleType Resource)
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

    const Array<HandleType>& GetItemsToDelete()
    {
        return this->DeletedItems[(this->ActiveDeletedItemsArray + 2) % 3];
    }

    void PrepareDeleteArraysForNextFrame()
    {
        u32 N2 = (this->ActiveDeletedItemsArray + 2) % 3;
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

} // namespace kraft::r
