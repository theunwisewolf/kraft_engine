#pragma once

#include <entt/entt.h>

namespace kraft {

struct World;

struct Entity
{
    EntityHandleT EntityHandle;

    Entity() = default;
    ~Entity()
    {}

    Entity(EntityHandleT handle, World* world) : EntityHandle(handle), world(world)
    {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... Arguments)
    {
        KASSERTM(!HasComponent<T>(), "Entity already has component");
        return world->Registry.emplace<T>(EntityHandle, std::forward<Args>(Arguments)...);
    }

    template<typename T>
    void RemoveComponent()
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        world->Registry.remove<T>(EntityHandle);
    }

    template<typename T>
    T& GetComponent()
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        return world->Registry.get<T>(EntityHandle);
    }

    template<typename T>
    const T& GetComponent() const
    {
        KASSERTM(HasComponent<T>(), "Entity does not have component");
        return world->Registry.get<T>(EntityHandle);
    }

    template<typename... T>
    decltype(auto) GetComponents()
    {
        KASSERTM(HasComponent<T...>(), "Entity does not have the components");
        return world->Registry.get<T...>(EntityHandle);
    }

    template<typename... T>
    bool HasComponent()
    {
        return world->Registry.all_of<T...>(EntityHandle);
    }

    template<typename... T>
    bool HasComponent() const
    {
        return world->Registry.all_of<T...>(EntityHandle);
    }

    KRAFT_INLINE const World* GetWorld() const
    {
        return this->world;
    }

    KRAFT_INLINE bool operator==(const Entity& other) const
    {
        return EntityHandle == other.EntityHandle && world == other.world;
    }

    KRAFT_INLINE bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

    KRAFT_INLINE EntityHandleT GetParent()
    {
        return world->Registry.get<RelationshipComponent>(EntityHandle).Parent;
    }

    KRAFT_INLINE Array<EntityHandleT>& GetChildren()
    {
        return world->Registry.get<RelationshipComponent>(EntityHandle).Children;
    }

    KRAFT_INLINE const Array<EntityHandleT>& GetChildren() const
    {
        return world->Registry.get<RelationshipComponent>(EntityHandle).Children;
    }

private:
    // The world this entity belongs to
    World* world;
};

}