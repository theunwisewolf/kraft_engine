#pragma once

#include <entt/entt.h>

namespace entt {
template<typename, typename>
class basic_registry;
}

namespace kraft {

struct Entity;
struct Material;

struct World
{
    using RegistryType = entt::basic_registry<kraft::EntityHandleT>;
    friend struct Entity;
    u64           EntityCount = 0;
    EntityHandleT Root;
    EntityHandleT GlobalLight = EntityHandleInvalid;
    Camera        Camera;

    World();
    ~World();

    Entity GetRoot() const;
    Entity GetEntity(EntityHandleT Handle) const;
    bool   IsValidEntity(EntityHandleT Handle) const;

    Entity CreateEntity();
    Entity CreateEntity(StringView Name, EntityHandleT Parent, Vec3f Position = Vec3fZero, Vec3f Rotation = Vec3fZero, Vec3f Scale = Vec3fOne);
    Entity CreateEntity(StringView Name, const Entity& Parent, Vec3f Position = Vec3fZero, Vec3f Rotation = Vec3fZero, Vec3f Scale = Vec3fOne);
    void   DestroyEntity(Entity Entity);

    void                       Render();
    Mat4f                      GetWorldSpaceTransformMatrix(Entity E);
    KRAFT_INLINE RegistryType& GetRegistry()
    {
        return Registry;
    }

private:
    RegistryType Registry;

    FlatHashMap<EntityHandleT, Entity> Entities;
};

} // namespace kraft