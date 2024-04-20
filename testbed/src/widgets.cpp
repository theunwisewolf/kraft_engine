#include "widgets.h"

#include "core/kraft_application.h"
#include "math/kraft_math.h"
#include "renderer/kraft_renderer_frontend.h"
#include "systems/kraft_texture_system.h"
#include "systems/kraft_material_system.h"

#include "utils.h"

#include <imgui/imgui.h>

static void* TextureID;

void InitImguiWidgets(const kraft::String& TexturePath)
{
    TextureID = kraft::Renderer->ImGuiRenderer.AddTexture(kraft::TextureSystem::AcquireTexture(TexturePath));
    kraft::Renderer->ImGuiRenderer.AddWidget("Debug", DrawImGuiWidgets);
}

void DrawImGuiWidgets(bool refresh)
{
    kraft::Camera& Camera = TestSceneState->SceneCamera;

    ImGui::Begin("Debug");
    TestSceneState->ObjectState.Dirty |= refresh;

    static float    left   = -(float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
                    right  = (float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
                    top    = -(float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
                    bottom = (float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
                    nearClip = -1.f, 
                    farClip = 1.f;

    static float fov = 45.f;
    static float width = (float)kraft::Application::Get()->Config.WindowWidth;
    static float height = (float)kraft::Application::Get()->Config.WindowHeight;
    static float nearClipP = 0.1f;
    static float farClipP = 1000.f;

    // To preserve the aspect ratio of the texture
    kraft::Texture* Texture = TestSceneState->ObjectState.Material->DiffuseMap.Texture;
    kraft::Vec2f ratio = { (float)Texture->Width / kraft::Application::Get()->Config.WindowWidth, (float)Texture->Height / kraft::Application::Get()->Config.WindowHeight };
    float downScale = kraft::math::Max(ratio.x, ratio.y);

    static kraft::Vec3f rotationDeg = kraft::Vec3fZero;
    static bool usePerspectiveProjection = TestSceneState->Projection == ProjectionType::Perspective;

    kraft::Camera& camera = TestSceneState->SceneCamera;
    if (ImGui::RadioButton("Perspective Projection", usePerspectiveProjection == true) || (TestSceneState->ObjectState.Dirty && usePerspectiveProjection))
    {
        usePerspectiveProjection = true;
        SetProjection(ProjectionType::Perspective);

        TestSceneState->ObjectState.Scale = {(float)Texture->Width / 20.f / downScale, (float)Texture->Height / 20.f / downScale, 1.0f};
        TestSceneState->ObjectState.Dirty = true;
    }

    if (ImGui::RadioButton("Orthographic Projection", usePerspectiveProjection == false) || (TestSceneState->ObjectState.Dirty && !usePerspectiveProjection))
    {
        usePerspectiveProjection = false;
        SetProjection(ProjectionType::Orthographic);

        TestSceneState->ObjectState.Scale = {(float)Texture->Width / downScale, (float)Texture->Height / downScale, 1.0f};
        TestSceneState->ObjectState.Dirty = true;
    }

    if (ImGui::ColorEdit4("Diffuse Color", TestSceneState->ObjectState.Material->DiffuseColor._data))
    {
        kraft::MaterialSystem::SetProperty(TestSceneState->ObjectState.Material, "DiffuseColor", TestSceneState->ObjectState.Material->DiffuseColor);
    }

    if (usePerspectiveProjection)
    {
        ImGui::Text("Perspective Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("FOV", &fov))
        {
            TestSceneState->ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Width", &width))
        {
            TestSceneState->ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Height", &height))
        {
            TestSceneState->ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Near clip", &nearClipP))
        {
            TestSceneState->ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Far clip", &farClipP))
        {
            TestSceneState->ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }
    }
    else
    {
        ImGui::Text("Orthographic Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("Left", &left))
        {
            TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Right", &right))
        {
            TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Top", &top))
        {
            TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Bottom", &bottom))
        {
            TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Near", &nearClip))
        {
            TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Far", &farClip))
        {
            TestSceneState->ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }
    }

    ImGui::PushID("CAMERA_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Camera Transform");
        ImGui::Separator();
        if (ImGui::DragFloat4("Translation", camera.Position))
        {
            camera.Dirty = true;
        }
    }
    ImGui::PopID();

    ImGui::PushID("OBJECT_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Separator();
        
        if (ImGui::DragFloat3("Scale", TestSceneState->ObjectState.Scale._data))
        {
            KINFO("Scale - %f, %f, %f", TestSceneState->ObjectState.Scale.x, TestSceneState->ObjectState.Scale.y, TestSceneState->ObjectState.Scale.z);
            TestSceneState->ObjectState.Dirty = true;
            // TestSceneState->ObjectState.ModelMatrix = ScaleMatrix(Vec3f{scale.x, scale.y, scale.z});
        }

        if (ImGui::DragFloat3("Translation", TestSceneState->ObjectState.Position._data))
        {
            KINFO("Translation - %f, %f, %f", TestSceneState->ObjectState.Position.x, TestSceneState->ObjectState.Position.y, TestSceneState->ObjectState.Position.z);
            TestSceneState->ObjectState.Dirty = true;
            // TestSceneState->ObjectState.ModelMatrix *= TranslationMatrix(position);
        }

        if (ImGui::DragFloat3("Rotation Degrees", rotationDeg._data))
        {
            TestSceneState->ObjectState.Rotation.x = kraft::DegToRadians(rotationDeg.x);
            TestSceneState->ObjectState.Rotation.y = kraft::DegToRadians(rotationDeg.y);
            TestSceneState->ObjectState.Rotation.z = kraft::DegToRadians(rotationDeg.z);
            TestSceneState->ObjectState.Dirty = true;
        }
    }
    ImGui::PopID();

    if (TestSceneState->ObjectState.Dirty)
    {
        TestSceneState->ObjectState.ModelMatrix = ScaleMatrix(TestSceneState->ObjectState.Scale) * RotationMatrixFromEulerAngles(TestSceneState->ObjectState.Rotation) * TranslationMatrix(TestSceneState->ObjectState.Position);
    }

    ImVec2 ViewPortPanelSize = ImGui::GetContentRegionAvail();
        //     {Vec3f(+0.5f, +0.5f, +0.0f), {1.f, 1.f}},
        // {Vec3f(-0.5f, -0.5f, +0.0f), {0.f, 0.f}},
        // {Vec3f(+0.5f, -0.5f, +0.0f), {1.f, 0.f}},
        // {Vec3f(-0.5f, +0.5f, +0.0f), {0.f, 1.f}},
    ImGui::Image(TextureID, ViewPortPanelSize, {0,1}, {1,0});

    ImGui::End();

    // static bool ShowDemoWindow = true;
    // ImGui::ShowDemoWindow(&ShowDemoWindow);

    TestSceneState->ObjectState.Dirty = false;
}