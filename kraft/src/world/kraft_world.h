#pragma once

#include <entt/entt.h>

// #include "world/kraft_entity.h"
#include "containers/kraft_hashmap.h"

namespace kraft {

struct Entity;

struct World
{
    uint64                            EntityIndex = 0;

    World();
    ~World();

    Entity CreateEntity();
    void   DestroyEntity(Entity Entity);

private:
    entt::registry                    Registry;
    FlatHashMap<entt::entity, Entity> Entities;
};
}