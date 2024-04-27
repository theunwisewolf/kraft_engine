#pragma once

#include "renderer/kraft_camera.h"
#include "renderer/kraft_renderer_frontend.h"
#include "scenes/simple_scene.h"

static void SetProjection(kraft::CameraProjectionType Type)
{
    kraft::Camera& Camera = TestSceneState->SceneCamera;
    Camera.Reset();
    if (Type == kraft::CameraProjectionType::Perspective)
    {
        Camera.ProjectionType = kraft::CameraProjectionType::Perspective;
        Camera.ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(45.0f), (float)kraft::Application::Get()->Config.WindowWidth / (float)kraft::Application::Get()->Config.WindowHeight, 0.1f, 1000.f);
        Camera.SetPosition({0.0f, 0.0f, 30.f});
    }
    else
    {
        Camera.ProjectionType = kraft::CameraProjectionType::Orthographic;
        Camera.ProjectionMatrix = kraft::OrthographicMatrix(
            -(float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
            (float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
            -(float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
            (float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
            -1.0f, 1.0f
        );

        Camera.SetPosition(kraft::Vec3fZero);
    }

    Camera.Dirty = true;
}

static void UpdateObjectScale()
{
    const kraft::Camera& Camera = TestSceneState->SceneCamera;
    const kraft::Material* Material = TestSceneState->GetSelectedEntity().MaterialInstance;
    kraft::Texture* Texture = Material->GetUniform<kraft::Texture*>("DiffuseSampler");

    kraft::Vec2f ratio = { (float)Texture->Width / kraft::Application::Get()->Config.WindowWidth, (float)Texture->Height / kraft::Application::Get()->Config.WindowHeight };
    float downScale = kraft::math::Max(ratio.x, ratio.y);
    if (Camera.ProjectionType == kraft::CameraProjectionType::Orthographic)
    {
        TestSceneState->GetSelectedEntity().Scale = {(float)Texture->Width / downScale, (float)Texture->Height / downScale, 1.0f};
    }
    else
    {
        TestSceneState->GetSelectedEntity().Scale = {(float)Texture->Width / 20.f / downScale, (float)Texture->Height / 20.f / downScale, 1.0f};
    }

    TestSceneState->GetSelectedEntity().ModelMatrix = ScaleMatrix(TestSceneState->GetSelectedEntity().Scale);
}