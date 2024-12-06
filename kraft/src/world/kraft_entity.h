#pragma once

#include <entt/entt.h>

#include "core/kraft_asserts.h"
#include "world/kraft_components.h"
#include "world/kraft_entity_types.h"
#include "world/kraft_world.h"

namespace kraft {

struct Entity
{
    EntityHandleT EntityHandle;

    Entity() = default;
    ~Entity()
    {}

    Entity(EntityHandleT EntityHandle, kraft::World* World) : EntityHandle(EntityHandle), World(World)
    {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... Arguments)
    {
        KASSERTM(!HasComponent<T>(), "Entity already has component");
        return World->GetRegistry().emplace<T>(EntityHandle, std::forward<Args>(Arguments)...);
    }

    template<typename T>
    void RemoveComponent()
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        World->GetRegistry().remove<T>(EntityHandle);
    }

    template<typename T>
    T& GetComponent()
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        return World->GetRegistry().get<T>(EntityHandle);
    }

    template<typename T>
    const T& GetComponent() const
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        return World->GetRegistry().get<T>(EntityHandle);
    }

    template<typename... T>
    decltype(auto) GetComponents()
    {
        KASSERTM(HasComponent<T...>(), "Entity does not have the components");
        return World->GetRegistry().get<T...>(EntityHandle);
    }

    template<typename... T>
    bool HasComponent()
    {
        return World->GetRegistry().all_of<T...>(EntityHandle);
    }

    template<typename... T>
    bool HasComponent() const
    {
        return World->GetRegistry().all_of<T...>(EntityHandle);
    }

    KRAFT_INLINE const World* GetWorld() const
    {
        return this->World;
    }

    KRAFT_INLINE bool operator==(const Entity& Other) const
    {
        return EntityHandle == Other.EntityHandle && World == Other.World;
    }

    KRAFT_INLINE bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

    KRAFT_INLINE EntityHandleT GetParent()
    {
        return World->GetRegistry().get<RelationshipComponent>(EntityHandle).Parent;
    }

    KRAFT_INLINE Array<EntityHandleT>& GetChildren()
    {
        return World->GetRegistry().get<RelationshipComponent>(EntityHandle).Children;
    }

    KRAFT_INLINE const Array<EntityHandleT>& GetChildren() const
    {
        return World->GetRegistry().get<RelationshipComponent>(EntityHandle).Children;
    }

private:
    // The world this entity belongs to
    World* World;
};

}