#pragma once

#include <platform/kraft_window.h>
#include <renderer/kraft_camera.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/kraft_resource_manager.h>
#include <world/kraft_components.h>
#include "editor.h"

static void SetProjection(kraft::CameraProjectionType Type)
{
    kraft::Camera& Camera = EditorState::Ptr->CurrentWorld->Camera;
    auto           Entity = EditorState::Ptr->GetSelectedEntity();
    // Camera.Reset();
    if (Type == kraft::CameraProjectionType::Perspective)
    {
        Camera.ProjectionType = kraft::CameraProjectionType::Perspective;
        Camera.ProjectionMatrix = kraft::PerspectiveMatrix(
            kraft::DegToRadians(45.0f), (float)kraft::Platform::GetWindow().Width / (float)kraft::Platform::GetWindow().Height, 0.1f, 1000.f
        );

        // Camera.SetPosition({25.0f, 15.0f, 20.f});
        // Camera.SetPitch(-0.381);

        kraft::Vec3f WorldUp = kraft::Vec3f(0.0f, 1.0f, 0.0f);
        kraft::Vec3f CameraTarget = Entity.GetComponent<kraft::TransformComponent>().Position;
        Camera.Position = { 0.0f, 0.0f, 30.f };
        Camera.UpdateVectors();
        // Camera.Front = kraft::Normalize(Camera.Position - CameraTarget);
        // Camera.Front = kraft::Vec3f({0.0f, 0.0f, -1.0f});
        // Camera.Right = kraft::Normalize(kraft::Cross(WorldUp, Camera.Front));
        // Camera.Up = kraft::Normalize(kraft::Cross(Camera.Right, Camera.Front));
    }
    else
    {
        Camera.ProjectionType = kraft::CameraProjectionType::Orthographic;
        Camera.ProjectionMatrix = kraft::OrthographicMatrix(
            -(float)kraft::Platform::GetWindow().Width * 0.5f,
            (float)kraft::Platform::GetWindow().Width * 0.5f,
            -(float)kraft::Platform::GetWindow().Height * 0.5f,
            (float)kraft::Platform::GetWindow().Height * 0.5f,
            -1.0f,
            1.0f
        );
    }

    Camera.Dirty = true;
}

static void UpdateObjectScale(uint32 EntityID)
{
    // SimpleObjectState&     Entity = TestSceneState->GetEntityByID(EntityID);
    // const kraft::Camera&   Camera = TestSceneState->SceneCamera;
    // const kraft::Material* Material = Entity.MaterialInstance;

    // kraft::renderer::Handle<kraft::Texture> Resource = Material->GetUniform<kraft::renderer::Handle<kraft::Texture>>("DiffuseSampler");
    // kraft::Texture*                         Texture = kraft::renderer::ResourceManager::Get()->GetTextureMetadata(Resource);

    // float WindowWidth = kraft::Platform::GetWindow().Width;
    // float WindowHeight = kraft::Platform::GetWindow().Height;
    // float RatioImage = Texture->Width / Texture->Height;
    // float RatioWindow = WindowWidth / WindowHeight;

    // kraft::Vec3f Scale;
    // kraft::Vec3f Size = (RatioWindow > RatioImage) ? (kraft::Vec3f{ Texture->Width * WindowHeight / Texture->Height, WindowHeight, 1.0f })
    //                                                : kraft::Vec3f{ WindowWidth, Texture->Height * WindowWidth / Texture->Width, 1.0f };
    // kraft::Vec2f ratio = { (float)Texture->Width / WindowWidth, (float)Texture->Height / WindowHeight };
    // float        downScale = kraft::math::Max(ratio.x, ratio.y);
    // if (Camera.ProjectionType == kraft::CameraProjectionType::Orthographic)
    // {
    //     Scale = { (float)Texture->Width / downScale, (float)Texture->Height / downScale, 1.0f };
    // }
    // else
    // {
    //     Scale = { (float)Texture->Width / 20.f / downScale, (float)Texture->Height / 20.f / downScale, 1.0f };
    // }

    // Entity.SetTransform(Entity.Position, Entity.Rotation, Scale);
}