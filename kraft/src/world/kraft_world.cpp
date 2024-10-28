#include "kraft_world.h"

#include "world/kraft_entity.h"

namespace kraft {

World::World()
{
    EntityIndex = 0;
}

World::~World()
{}

Entity World::CreateEntity()
{
    entt::entity EntityHandle = Registry.create();
    Entity NewEntity = Entity(EntityHandle, this);
    this->Entities[EntityHandle] = NewEntity;
    this->EntityIndex++;

    return NewEntity;
}

void World::DestroyEntity(Entity Entity)
{
    Registry.destroy(Entity.EntityHandle);
}

}