#pragma once

// #include <entt/entt.h>

// #include "world/kraft_entity.h"
#include "containers/kraft_hashmap.h"
#include "core/kraft_core.h"
#include "core/kraft_string_view.h"
#include "world/kraft_entity_types.h"
#include "renderer/kraft_camera.h"

namespace entt {
template<typename, typename>
class basic_registry;

// using registry_ = basic_registry<kraft::EntityHandleT, std::allocator<kraft::EntityHandleT>>;
}

namespace kraft {

struct Entity;

// using Registry = entt::basic_registry;

struct World
{
    friend struct Entity;
    uint64        EntityCount = 0;
    EntityHandleT Root;
    Camera        Camera;

    World();
    ~World();

    Entity        CreateEntity();
    const Entity& GetRoot() const;
    Entity
    CreateEntity(StringView Name, EntityHandleT Parent, Vec3f Position = Vec3fZero, Vec3f Rotation = Vec3fZero, Vec3f Scale = Vec3fOne);
    Entity
    CreateEntity(StringView Name, const Entity& Parent, Vec3f Position = Vec3fZero, Vec3f Rotation = Vec3fZero, Vec3f Scale = Vec3fOne);
    void    DestroyEntity(Entity Entity);
    Entity& GetEntity(EntityHandleT Handle);

    void  Render();
    Mat4f GetWorldSpaceTransformMatrix(Entity E);

private:
    entt::basic_registry<kraft::EntityHandleT, std::allocator<kraft::EntityHandleT>>& GetRegistry() const;
    FlatHashMap<EntityHandleT, Entity>                                                Entities;
};

}