#pragma once

#include <entt/entt.h>

#include "world/kraft_components.h"
#include "world/kraft_world.h"

namespace kraft {

struct Entity
{
    entt::entity EntityHandle;

    Entity() = default;
    ~Entity() {}

    Entity(entt::entity EntityHandle, kraft::World* World) : World(World), EntityHandle(EntityHandle)
    {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... Arguments)
    {
        return World->Registry.emplace<T>(EntityHandle, std::forward<Args>(Arguments)...);
    }

    template<typename T>
    T& GetComponent()
    {
        return World->Registry.get<T>(EntityHandle);
    }

    const kraft::World* GetWorld() const
    {
        return this->World;
    }

private:
    // The world this entity belongs to
    kraft::World* World;
};

}