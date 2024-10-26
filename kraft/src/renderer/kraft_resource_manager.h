#pragma once

#include <core/kraft_core.h>
#include <core/kraft_asserts.h>
#include <math/kraft_math.h>
#include <renderer/kraft_renderer_types.h>

namespace kraft
{
    struct Texture;
}

namespace kraft::renderer
{

struct TempBuffer
{
    Handle<Buffer> GPUBuffer;
    uint8* Ptr; // Location of data inside the GPUBuffer
    uint64 Offset; // Offset from the start of the buffer
};

class TempMemoryBlockAllocator
{
public:
    struct Block
    {
        uint8* Ptr;
        Handle<Buffer> GPUBuffer;
    };

    uint64 BlockSize;
    uint64 AllocationCount = 0;
    virtual Block GetNextFreeBlock() = 0;
};

struct TempAllocator
{
    uint64                           CurrentOffset;
    TempMemoryBlockAllocator::Block  CurrentBlock;
    TempMemoryBlockAllocator*        Allocator;

    TempBuffer Allocate(uint64 Size, uint64 Alignment)
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

        Allocator->AllocationCount++;
        KDEBUG("Allocation Count: %d", Allocator->AllocationCount);
        return {
            .GPUBuffer = CurrentBlock.GPUBuffer,
            .Ptr = CurrentBlock.Ptr + Offset,
            .Offset = Offset,
        };
    }
};


template<typename ConcreteType, typename Type>
struct Pool
{
    using HandleType = Handle<Type>;
    typedef void (*PoolCleanupFunction)(ConcreteType* GPUResource);

    uint16        PoolSize = 0;
    uint16        FreeListTop;
    Type*         AuxiliaryData;
    ConcreteType* Data;
    HandleType*   Handles;
    uint16*       FreeList;

    Array<HandleType> DeletedItems[3] = {};
    uint32 ActiveDeletedItemsArray = 0;

    Pool(uint16 ElementCount) :
        FreeListTop(0)
    {
        KASSERT(ElementCount > 0);
        this->Grow(ElementCount);
    }

    void Grow(uint16 ElementCount)
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
        
        uint8* OldBuffer = (uint8*)this->AuxiliaryData;
        Type* OldAuxiliaryData = (Type*)OldBuffer;
        ConcreteType* OldData = (ConcreteType*)(OldBuffer + OldAuxiliaryDataArraySize);
        HandleType* OldHandles = (HandleType*)(OldBuffer + OldAuxiliaryDataArraySize + OldDataArraySize);
        uint16* OldFreeList = (uint16*)(OldBuffer + OldAuxiliaryDataArraySize + OldDataArraySize + OldHandlesArraySize);

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

        uint16 Index = this->FreeList[this->FreeListTop++];
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

    uint64 GetSize() const { return this->PoolSize; }

    void MarkForDelete(HandleType Handle)
    {
        if (!ValidateHandle(Handle)) return;

        uint16 Index = Handle.Index;
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

        this->FreeList[this->FreeListTop-1] = Resource.Index;
        this->FreeListTop--;
    }

    const Array<HandleType>& GetItemsToDelete()
    {
        return this->DeletedItems[(this->ActiveDeletedItemsArray+2) % 3];
    }

    void PrepareDeleteArraysForNextFrame()
    {
        uint32 N2 = (this->ActiveDeletedItemsArray+2) % 3;
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

struct Buffer;

class ResourceManager
{
protected:
    TempAllocator TempGPUAllocator;
    PhysicalDeviceFormatSpecs FormatSpecs;

public:
    static ResourceManager *Ptr;

    KRAFT_INLINE static ResourceManager* Get() { return Ptr; }

    ResourceManager();
    virtual ~ResourceManager() = 0;

    // Creation apis
    virtual Handle<Texture> CreateTexture(TextureDescription Description) = 0;
    virtual Handle<Buffer> CreateBuffer(BufferDescription Description) = 0;
    virtual Handle<RenderPass> CreateRenderPass(RenderPassDescription Description) = 0;

    // Destruction apis
    virtual void DestroyTexture(Handle<Texture> Resource) = 0;
    virtual void DestroyBuffer(Handle<Buffer> Resource) = 0;
    virtual void DestroyRenderPass(Handle<RenderPass> Resource) = 0;

    // Access apis
    virtual uint8* GetBufferData(Handle<Buffer> Buffer) = 0;

    // Creates a temporary buffer that gets destroyed at the end of the frame
    virtual TempBuffer CreateTempBuffer(uint64 Size) = 0;

    // Uploads a raw buffer data to the GPU
    virtual bool UploadTexture(Handle<Texture> Texture, Handle<Buffer> Buffer, uint64 BufferOffset) = 0;
    virtual bool UploadBuffer(UploadBufferDescription Description) = 0;
    
    KRAFT_INLINE void SetPhysicalDeviceFormatSpecs(PhysicalDeviceFormatSpecs Specs) { FormatSpecs = Specs; }
    KRAFT_INLINE const PhysicalDeviceFormatSpecs& GetPhysicalDeviceFormatSpecs() const { return FormatSpecs; }

    virtual Texture* GetTextureMetadata(Handle<Texture> Resource) = 0;

    virtual void StartFrame(uint64 FrameNumber) = 0;
    virtual void EndFrame(uint64 FrameNumber) = 0;
};

};