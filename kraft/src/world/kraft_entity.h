#pragma once

#include <entt/entt.h>

#include "core/kraft_asserts.h"
#include "world/kraft_components.h"
#include "world/kraft_world.h"

namespace kraft {

struct Entity
{
    entt::entity EntityHandle;

    Entity() = default;
    ~Entity()
    {}

    Entity(entt::entity EntityHandle, kraft::World* World) : World(World), EntityHandle(EntityHandle)
    {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... Arguments)
    {
        KASSERTM(!HasComponent<T>(), "Entity already has component");
        return World->Registry.emplace<T>(EntityHandle, std::forward<Args>(Arguments)...);
    }

    template<typename T>
    void RemoveComponent()
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        World->Registry.remove<T>(EntityHandle);
    }

    template<typename T>
    T& GetComponent()
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        return World->Registry.get<T>(EntityHandle);
    }

    template<typename T>
    const T& GetComponent() const
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        return World->Registry.get<T>(EntityHandle);
    }

    template<typename... T>
    decltype(auto) GetComponents()
    {
        KASSERTM(HasComponent<T...>(), "Entity does not have the components");
        return World->Registry.get<T...>(EntityHandle);
    }

    template<typename... T>
    bool HasComponent()
    {
        return World->Registry.all_of<T...>(EntityHandle);
    }

    template<typename... T>
    bool HasComponent() const
    {
        return World->Registry.all_of<T...>(EntityHandle);
    }

    const World* GetWorld() const
    {
        return this->World;
    }

    bool operator==(const Entity& Other) const
    {
        return EntityHandle == Other.EntityHandle && World == Other.World;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

private:
    // The world this entity belongs to
    World* World;

    friend class World;
};

}