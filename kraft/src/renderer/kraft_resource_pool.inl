#pragma once

namespace kraft::r {
template <typename T> struct Handle;
}

namespace kraft {
template <typename T> struct Array;
}

namespace kraft::r {

template <typename ConcreteType, typename Type> struct Pool {
    using HandleType = Handle<Type>;
    typedef void (*PoolCleanupFunction)(ConcreteType* resource);

    u16 pool_size = 0;
    u16 free_list_top;
    u64 allocated_size_in_bytes = 0;
    Type* auxiliary_data;
    ConcreteType* data;
    HandleType* handles;
    u16* free_list;

    Array<HandleType> deleted_items[3] = {};
    u32 active_deleted_items_array = 0;

    void Grow(u16 element_count) {
        // For now we only support enlarging the pool
        KASSERT(element_count > this->pool_size);

        // Allocate a new buffer and save any old buffer references
        u64 auxiliary_data_array_size = sizeof(Type) * element_count;
        u64 data_array_size = sizeof(ConcreteType) * element_count;
        u64 handles_array_size = sizeof(HandleType) * element_count;
        u64 free_list_array_size = sizeof(u16) * element_count;
        u64 new_size = auxiliary_data_array_size + data_array_size + handles_array_size + free_list_array_size;
        u8* new_buffer = (u8*)Malloc(new_size, MEMORY_TAG_RESOURCE_POOL, true);

        u64 old_auxiliary_data_array_size = sizeof(Type) * this->pool_size;
        u64 old_data_array_size = sizeof(ConcreteType) * this->pool_size;
        u64 old_handles_array_size = sizeof(HandleType) * this->pool_size;
        u64 old_free_list_array_size = sizeof(u16) * this->pool_size;
        u64 old_size = this->allocated_size_in_bytes;
        this->allocated_size_in_bytes = new_size;

        u8* old_buffer = (u8*)this->auxiliary_data;
        Type* old_auxiliary_data = (Type*)old_buffer;
        ConcreteType* old_data = (ConcreteType*)(old_buffer + old_auxiliary_data_array_size);
        HandleType* old_handles = (HandleType*)(old_buffer + old_auxiliary_data_array_size + old_data_array_size);
        u16* old_free_list = (u16*)(old_buffer + old_auxiliary_data_array_size + old_data_array_size + old_handles_array_size);

        this->auxiliary_data = (Type*)new_buffer;
        this->data = (ConcreteType*)(new_buffer + auxiliary_data_array_size);
        this->handles = (HandleType*)(new_buffer + auxiliary_data_array_size + data_array_size);
        this->free_list = (u16*)(new_buffer + auxiliary_data_array_size + data_array_size + handles_array_size);

        // Now if we had any old data, copy it over
        if (this->pool_size > 0) {
            MemCpy(auxiliary_data, old_auxiliary_data, old_auxiliary_data_array_size);
            MemCpy(data, old_data, old_data_array_size);
            MemCpy(handles, old_handles, old_handles_array_size);
            MemCpy(free_list, old_free_list, old_free_list_array_size);

            Free(old_buffer, old_size, MEMORY_TAG_RESOURCE_POOL);
        }

        // Mark the remaining elements as free
        for (int i = this->pool_size; i < element_count; i++) {
            this->free_list[i] = i;
        }

        this->pool_size = element_count;
    }

    void Destroy() {
        u64 auxiliary_data_array_size = sizeof(Type) * this->pool_size;
        u64 data_array_size = sizeof(ConcreteType) * this->pool_size;
        u64 handles_array_size = sizeof(HandleType) * this->pool_size;
        u64 free_list_array_size = sizeof(u16) * this->pool_size;
        u64 size = auxiliary_data_array_size + data_array_size + handles_array_size + free_list_array_size;

        Free(this->auxiliary_data, size, MEMORY_TAG_RESOURCE_POOL);
    }

    bool ValidateHandle(HandleType handle) const {
        KASSERT(handle.index < this->pool_size);
        if (handle.index < this->pool_size) {
            // Data has changed?
            HandleType current_handle = this->handles[handle.index];
            return current_handle.generation == handle.generation;
        }

        return false;
    }

    Type* GetAuxiliaryData(HandleType handle) const {
        return ValidateHandle(handle) ? &this->auxiliary_data[handle.index] : nullptr;
    }

    ConcreteType* Get(HandleType handle) const {
        return ValidateHandle(handle) ? &this->data[handle.index] : nullptr;
    }

    HandleType Insert(Type auxiliary_data, ConcreteType data) {
        if (this->free_list_top >= this->pool_size) {
            this->Grow(this->pool_size * 2);
        }

        u16 index = this->free_list[this->free_list_top++];
        HandleType& handle = this->handles[index];
        handle.index = index;
        handle.generation++;
        // Skip generation 0 - The sentinel!
        if (handle.generation == 0)
            handle.generation = 1;

        this->auxiliary_data[handle.index] = auxiliary_data;
        this->data[handle.index] = data;

        return handle;
    }

    void* Allocate(HandleType handle) {
        return (void*)(&this->data[handle.index]);
    }

    u64 GetSize() const {
        return this->free_list_top;
    }

    u64 GetCapacity() const {
        return this->pool_size;
    }

    // Iterates over all alive entries in the pool
    // Callback signature: void(u16 index, ConcreteType* resource, Type* metadata)
    template <typename Fn>
    void ForEach(Fn callback) {
        for (u16 i = 0; i < this->pool_size; i++) {
            if (this->handles[i].generation != 0) {
                callback(i, &this->data[i], &this->auxiliary_data[i]);
            }
        }
    }

    void MarkForDelete(HandleType handle) {
        if (!ValidateHandle(handle))
            return;

        u16 index = handle.index;
        this->handles[index].generation = 0; // Invalidate the handle
        this->deleted_items[this->active_deleted_items_array].Push(this->handles[index]);
    }

    void Delete(HandleType handle) {
        if (handle.generation != 0) {
            KERROR("Deleting a handle with generation > 0 | Index = %d, Generation = %d", handle.index, handle.generation);
            return;
        }

        // this->Data[Resource.Index] = {};
        // this->AuxiliaryData[Resource.Index] = {};

        KASSERT(this->free_list_top > 0);

        this->free_list[this->free_list_top - 1] = handle.index;
        this->free_list_top--;
    }

    const Array<HandleType>& GetItemsToDelete() {
        return this->deleted_items[(this->active_deleted_items_array + 2) % 3];
    }

    void PrepareDeleteArraysForNextFrame() {
        u32 n2 = (this->active_deleted_items_array + 2) % 3;
        this->deleted_items[n2].Clear();

        this->active_deleted_items_array = (this->active_deleted_items_array + 1) % 3;
    }

    // Removes any resources pending deletion
    void Cleanup(PoolCleanupFunction callback) {
        const auto& resources_to_delete = this->GetItemsToDelete();
        for (int i = 0; i < resources_to_delete.Size(); i++) {
            ConcreteType* resource = &this->data[resources_to_delete[i].index];
            callback(resource);

            this->Delete(resources_to_delete[i]);
        }

        this->PrepareDeleteArraysForNextFrame();
    }
};

} // namespace kraft::r
