#pragma once

#include <core/kraft_allocators.h>

namespace kraft {

template<typename T, u32 ChunkSize = 16>
struct ChunkedArray
{
    struct Chunk
    {
        T      data[ChunkSize];
        u32    count = 0;
        Chunk* next = nullptr;
    };

    ArenaAllocator* arena = nullptr;
    Chunk*          first = nullptr;
    Chunk*          last = nullptr;
    u32             total_count = 0;

    void Init(ArenaAllocator* arena)
    {
        this->arena = arena;
        first = nullptr;
        last = nullptr;
        total_count = 0;
    }

    T* Push(const T& value)
    {
        if (!last || last->count >= ChunkSize)
        {
            Chunk* chunk = ArenaPush(arena, Chunk);
            chunk->count = 0;
            chunk->next = nullptr;

            if (last)
            {
                last->next = chunk;
            }
            else
            {
                first = chunk;
            }

            last = chunk;
        }

        T* slot = &last->data[last->count];
        *slot = value;
        last->count++;
        total_count++;

        return slot;
    }

    bool SwapRemove(u32 global_index)
    {
        if (global_index >= total_count)
        {
            return false;
        }

        // Find the chunk and local index for the element to remove
        Chunk* target_chunk = this->first;
        u32    remaining = global_index;
        while (remaining >= target_chunk->count)
        {
            remaining -= target_chunk->count;
            target_chunk = target_chunk->next;
        }

        u32    local_index = remaining;
        Chunk* last_chunk = this->last;

        // If removing the last element overall, just decrement
        if (target_chunk == last_chunk && local_index == last_chunk->count - 1)
        {
            last_chunk->count--;
            total_count--;

            if (last_chunk->count == 0 && last_chunk != first)
            {
                Chunk* prev = this->first;
                while (prev->next != last_chunk)
                {
                    prev = prev->next;
                }

                prev->next = nullptr;
                this->last = prev;
            }

            return true;
        }

        // Swap with the last element
        target_chunk->data[local_index] = last_chunk->data[last_chunk->count - 1];
        last_chunk->count--;
        total_count--;

        if (last_chunk->count == 0 && last_chunk != first)
        {
            Chunk* prev = first;
            while (prev->next != last_chunk)
            {
                prev = prev->next;
            }

            prev->next = nullptr;
            last = prev;
        }

        return true;
    }
};

} // namespace kraft

// Iterates over all elements in a ChunkedArray
// Declares a pointer `it` of the element type
// Usage:
//   ca_for(my_array, it) {
//       it->some_field = ...;
//   }
#define ca_for(ca, it)                                                                                                                                                                                 \
    for (auto* _ca_chunk = (ca).first; _ca_chunk; _ca_chunk = _ca_chunk->next)                                                                                                                         \
        for (u32 _ca_i = 0; _ca_i < _ca_chunk->count && ((it) = &_ca_chunk->data[_ca_i], 1); _ca_i++)
