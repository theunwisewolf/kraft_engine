#pragma once

#include "renderer/kraft_renderer_frontend.h"
#include "renderer/vulkan/tests/simple_scene.h"

static void SetProjection(kraft::SceneState::ProjectionType Type)
{
    kraft::renderer::SceneState* TestSceneState = kraft::renderer::GetSceneState();
    kraft::Camera& camera = TestSceneState->SceneCamera;
    camera.Reset();
    if (Type == kraft::SceneState::ProjectionType::Perspective)
    {
        TestSceneState->Projection = kraft::SceneState::ProjectionType::Perspective;
        TestSceneState->ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(45.0f), (float)kraft::Application::Get()->Config.WindowWidth / (float)kraft::Application::Get()->Config.WindowHeight, 0.1f, 1000.f);

        camera.SetPosition({0.0f, 0.0f, 30.f});
    }
    else
    {
        TestSceneState->Projection = kraft::SceneState::ProjectionType::Orthographic;
        TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(
            -(float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
            (float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
            -(float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
            (float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
            -1.0f, 1.0f
        );

        camera.SetPosition(kraft::Vec3fZero);
    }

    camera.Dirty = true;
}

static void UpdateObjectScale()
{
    kraft::renderer::SceneState* TestSceneState = kraft::renderer::GetSceneState();
    kraft::Texture* Texture = TestSceneState->ObjectState.Material->DiffuseMap.Texture; 
    kraft::Vec2f ratio = { (float)Texture->Width / kraft::Application::Get()->Config.WindowWidth, (float)Texture->Height / kraft::Application::Get()->Config.WindowHeight };
    float downScale = kraft::math::Max(ratio.x, ratio.y);
    if (TestSceneState->Projection == kraft::SceneState::ProjectionType::Orthographic)
    {
        TestSceneState->ObjectState.Scale = {(float)Texture->Width / downScale, (float)Texture->Height / downScale, 1.0f};
    }
    else
    {
        TestSceneState->ObjectState.Scale = {(float)Texture->Width / 20.f / downScale, (float)Texture->Height / 20.f / downScale, 1.0f};
    }

    TestSceneState->ObjectState.ModelMatrix = ScaleMatrix(TestSceneState->ObjectState.Scale);
}