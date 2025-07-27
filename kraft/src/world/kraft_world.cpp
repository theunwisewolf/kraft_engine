#include "kraft_world.h"

#include <containers/kraft_array.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/kraft_renderer_types.h>
#include <world/kraft_components.h>
#include <world/kraft_entity.h>

// static entt::basic_registry<kraft::EntityHandleT> Registry = entt::basic_registry<kraft::EntityHandleT>();
// static kraft::Entity                              WorldRoot = kraft::Entity();

namespace kraft {

World::World()
{
    EntityCount = 0;
    Entity WorldRootEntity = this->CreateEntity("WorldRoot", EntityHandleInvalid);
    this->Root = WorldRootEntity.EntityHandle;
    this->Camera = kraft::Camera();
}

World::~World()
{
    this->Registry.clear();
}

Entity World::GetRoot() const
{
    return this->Entities.at(this->Root);
}

Entity World::CreateEntity()
{
    EntityHandleT EntityHandle = Registry.create();
    Entity        NewEntity = Entity(EntityHandle, this);
    this->Entities[EntityHandle] = NewEntity;
    this->EntityCount++;

    return NewEntity;
}

Entity World::CreateEntity(StringView Name, EntityHandleT Parent, Vec3f Position, Vec3f Rotation, Vec3f Scale)
{
    EntityHandleT EntityHandle = Registry.create();
    Entity        NewEntity = Entity(EntityHandle, this);
    this->Entities[EntityHandle] = NewEntity;
    this->EntityCount++;

    Registry.emplace<MetadataComponent>(EntityHandle, Name);
    Registry.emplace<RelationshipComponent>(EntityHandle, Parent);
    Registry.emplace<TransformComponent>(EntityHandle, Position, Rotation, Scale);
    if (Parent != EntityHandleInvalid)
    {
        RelationshipComponent& Relationship = Registry.get<RelationshipComponent>(Parent);
        Relationship.Children.Push(EntityHandle);
    }

    return NewEntity;
}

Entity World::CreateEntity(StringView Name, const Entity& Parent, Vec3f Position, Vec3f Rotation, Vec3f Scale)
{
    return this->CreateEntity(Name, Parent.EntityHandle, Position, Rotation, Scale);
}

void World::DestroyEntity(Entity Entity)
{
    // RelationshipComponent& Relationship = Registry.get<RelationshipComponent>(Entity.EntityHandle);
    RelationshipComponent& ParentRelationship = Registry.get<RelationshipComponent>(Entity.GetParent());

    for (int i = 0; i < ParentRelationship.Children.Length; i++)
    {
        if (ParentRelationship.Children[i] == Entity.EntityHandle)
        {
            ParentRelationship.Children.Pop(i);
            break;
        }
    }

    Registry.destroy(Entity.EntityHandle);
}

Entity World::GetEntity(EntityHandleT Handle) const
{
    KASSERT(Handle != EntityHandleInvalid);
    return Entities.at(Handle);
}

bool World::IsValidEntity(EntityHandleT Handle) const
{
    return Entities.contains(Handle);
}

void World::Render()
{
    g_Renderer->Camera = &this->Camera;
    // kraft::renderer::Renderer->CurrentWorld = this;

    auto Group = Registry.group<MeshComponent, TransformComponent>();
    for (auto EntityHandle : Group)
    {
        auto [Transform, Mesh] = Group.get<TransformComponent, MeshComponent>(EntityHandle);
        g_Renderer->AddRenderable(kraft::renderer::Renderable{
            .ModelMatrix = Transform.ModelMatrix,// GetWorldSpaceTransformMatrix(Entity(EntityHandle, this)),
            .MaterialInstance = Mesh.MaterialInstance,
            .GeometryId = Mesh.GeometryID,
            .EntityId = (uint32)EntityHandle,
        });
    }
}

Mat4f World::GetWorldSpaceTransformMatrix(Entity E)
{
    TransformComponent Transform = E.GetComponent<TransformComponent>();
    EntityHandleT      ParentEntityHandle = E.GetParent();
    if (ParentEntityHandle == EntityHandleInvalid)
    {
        return Transform.ModelMatrix;
    }

    Entity ParentEntity = this->Entities[ParentEntityHandle];
    Mat4f  ParentTransformMatrix = GetWorldSpaceTransformMatrix(ParentEntity);

    return Transform.ModelMatrix * ParentTransformMatrix;
    // return ParentTransformMatrix * Transform.ModelMatrix;
}

}