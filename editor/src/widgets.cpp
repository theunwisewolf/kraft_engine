#include "widgets.h"

#include "core/kraft_engine.h"
#include "math/kraft_math.h"
#include "renderer/kraft_renderer_frontend.h"
#include "resources/kraft_resource_types.h"
#include "systems/kraft_material_system.h"
#include "systems/kraft_texture_system.h"

#include "editor.h"
#include "utils.h"
#include "imgui/imgui_renderer.h"

#include <renderer/kraft_resource_manager.h>
#include <platform/kraft_window.h>

#include "imgui/extensions/imguizmo/ImGuizmo.h"
#include <imgui/imgui.h>

struct GizmosState
{
    ImGuizmo::OPERATION CurrentOperation;
    ImGuizmo::MODE      Mode;
    bool                Snap;
} State = {};

static kraft::renderer::Handle<kraft::Texture> SceneTexture;
static ImTextureID                             TextureID;
static ImTextureID                             ImSceneTexture;

void InitImguiWidgets(kraft::renderer::Handle<kraft::Texture> DiffuseTexture)
{
    TextureID = GlobalAppState.ImGuiRenderer->AddTexture(DiffuseTexture);
    GlobalAppState.ImGuiRenderer->AddWidget("Debug", DrawImGuiWidgets);

    SceneTexture = kraft::Renderer->GetSceneViewTexture();
    ImSceneTexture = GlobalAppState.ImGuiRenderer->AddTexture(SceneTexture);

    State.CurrentOperation = ImGuizmo::TRANSLATE;
    State.Mode = ImGuizmo::LOCAL;
    State.Snap = false;
}

void DrawImGuiWidgets(bool refresh)
{
    kraft::Camera& Camera = TestSceneState->SceneCamera;

    ImGui::Begin("Debug");

    static float left = -(float)kraft::Platform::GetWindow().Width * 0.5f,
                 right = (float)kraft::Platform::GetWindow().Width * 0.5f,
                 top = -(float)kraft::Platform::GetWindow().Height * 0.5f,
                 bottom = (float)kraft::Platform::GetWindow().Height * 0.5f, nearClip = -1.f, farClip = 1.f;

    static float fov = 45.f;
    static float width = (float)kraft::Platform::GetWindow().Width;
    static float height = (float)kraft::Platform::GetWindow().Height;
    static float nearClipP = 0.1f;
    static float farClipP = 1000.f;

    // To preserve the aspect ratio of the texture
    kraft::renderer::Handle<kraft::Texture> Resource =
        TestSceneState->GetSelectedEntity().MaterialInstance->GetUniform<kraft::renderer::Handle<kraft::Texture>>("DiffuseSampler");
    kraft::Texture* Texture = kraft::renderer::ResourceManager::Get()->GetTextureMetadata(Resource);

    kraft::Vec2f ratio = { (float)Texture->Width / kraft::Platform::GetWindow().Width,
                           (float)Texture->Height / kraft::Platform::GetWindow().Height };
    float        downScale = kraft::math::Max(ratio.x, ratio.y);

    static kraft::Vec3f rotationDeg = kraft::Vec3fZero;
    static bool         usePerspectiveProjection = Camera.ProjectionType == kraft::CameraProjectionType::Perspective;

    kraft::Vec3f Translation = TestSceneState->GetSelectedEntity().Position;
    kraft::Vec3f Rotation = TestSceneState->GetSelectedEntity().Rotation;
    kraft::Vec3f Scale = TestSceneState->GetSelectedEntity().Scale;
    rotationDeg.x = kraft::RadiansToDegrees(Rotation.x);
    rotationDeg.y = kraft::RadiansToDegrees(Rotation.y);
    rotationDeg.z = kraft::RadiansToDegrees(Rotation.z);

    if (ImGui::RadioButton("Perspective Projection", usePerspectiveProjection == true))
    {
        usePerspectiveProjection = true;
        SetProjection(kraft::CameraProjectionType::Perspective);

        Scale = { (float)Texture->Width / 20.f / downScale, (float)Texture->Height / 20.f / downScale, 1.0f };
    }

    if (ImGui::RadioButton("Orthographic Projection", usePerspectiveProjection == false))
    {
        usePerspectiveProjection = false;
        SetProjection(kraft::CameraProjectionType::Orthographic);

        Scale = { (float)Texture->Width / downScale, (float)Texture->Height / downScale, 1.0f };
    }

    auto                Instance = TestSceneState->GetSelectedEntity().MaterialInstance;
    static kraft::Vec4f DiffuseColor = Instance->GetUniform<kraft::Vec4f>("DiffuseColor");
    if (ImGui::ColorEdit4("Diffuse Color", DiffuseColor))
    {
        kraft::MaterialSystem::SetProperty(Instance, "DiffuseColor", DiffuseColor);
    }

    if (usePerspectiveProjection)
    {
        ImGui::Text("Perspective Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("FOV", &fov))
        {
            Camera.ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Width", &width))
        {
            Camera.ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Height", &height))
        {
            Camera.ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Near clip", &nearClipP))
        {
            Camera.ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Far clip", &farClipP))
        {
            Camera.ProjectionMatrix = kraft::PerspectiveMatrix(kraft::DegToRadians(fov), width / height, nearClipP, farClipP);
        }
    }
    else
    {
        ImGui::Text("Orthographic Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("Left", &left))
        {
            Camera.ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Right", &right))
        {
            Camera.ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Top", &top))
        {
            Camera.ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Bottom", &bottom))
        {
            Camera.ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Near", &nearClip))
        {
            Camera.ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Far", &farClip))
        {
            Camera.ProjectionMatrix = kraft::OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }
    }

    ImGui::PushID("CAMERA_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Camera Transform");
        ImGui::Separator();
        if (ImGui::DragFloat4("Translation", Camera.Position))
        {
            Camera.Dirty = true;
        }

        if (ImGui::DragFloat("Pitch", &Camera.Pitch))
        {
            Camera.UpdateVectors();
        }

        if (ImGui::DragFloat("Yaw", &Camera.Yaw))
        {
            Camera.UpdateVectors();
        }
    }
    ImGui::PopID();

    ImGui::PushID("OBJECT_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Separator();

        if (ImGui::DragFloat3("Translation", Translation._data))
        {
            KINFO("Translation - %f, %f, %f", Translation.x, Translation.y, Translation.z);
        }

        if (ImGui::DragFloat3("Rotation", rotationDeg._data))
        {
            Rotation.x = kraft::DegToRadians(rotationDeg.x);
            Rotation.y = kraft::DegToRadians(rotationDeg.y);
            Rotation.z = kraft::DegToRadians(rotationDeg.z);
        }

        if (ImGui::DragFloat3("Scale", Scale._data))
        {
            KINFO("Scale - %f, %f, %f", Scale.x, Scale.y, Scale.z);
        }
    }
    ImGui::PopID();

    ImVec2 ViewPortPanelSize = ImGui::GetContentRegionAvail();
    // ImGui::Image(SceneTexture, ViewPortPanelSize, {0,1}, {1,0});

    ImGui::End();

    // ImGui::SetNextWindowSize({1024, 768});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    ViewPortPanelSize = ImGui::GetContentRegionAvail();
    ImGui::GetForegroundDrawList()->AddRect(
        ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + ViewPortPanelSize.y),
        ImVec2(ImGui::GetWindowPos().x + ViewPortPanelSize.x, ImGui::GetWindowPos().y),
        ImGui::GetColorU32(ImVec4(1, 0, 0, 1))
    );

    if (kraft::Renderer->SetSceneViewViewportSize(ViewPortPanelSize.x, ViewPortPanelSize.y))
    {
        // Remove the old texture
        GlobalAppState.ImGuiRenderer->RemoveTexture(ImSceneTexture);
        ImSceneTexture = GlobalAppState.ImGuiRenderer->AddTexture(kraft::Renderer->GetSceneViewTexture());

        if (usePerspectiveProjection)
        {
            Camera.SetPerspectiveProjection(kraft::DegToRadians(fov), ViewPortPanelSize.x, ViewPortPanelSize.y, nearClipP, farClipP);
        }
        else
        {
            Camera.SetOrthographicProjection(ViewPortPanelSize.x, ViewPortPanelSize.y, nearClip, farClip);
        }
    }
    else
    {
        ImGui::Image(ImSceneTexture, ViewPortPanelSize, { 0, 1 }, { 1, 0 });
    }

    // auto CurrentSceneTexture = kraft::Renderer->GetSceneViewTexture();
    // if (CurrentSceneTexture->Width != SceneTexture->Width || CurrentSceneTexture->Height != SceneTexture->Height)
    // {
    //     GlobalAppState.ImGuiRenderer->RemoveTexture(ImSceneTexture);
    //     SceneTexture = CurrentSceneTexture;
    //     ImSceneTexture = GlobalAppState.ImGuiRenderer->AddTexture(SceneTexture);
    // }

    // EditTransform(Camera, TestSceneState->GetSelectedEntity().ModelMatrix);
    {
        auto& SelectedEntity = TestSceneState->GetSelectedEntity();
        if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Q))
            State.CurrentOperation = ImGuizmo::TRANSLATE;

        if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_W))
            State.CurrentOperation = ImGuizmo::ROTATE;

        if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_E))
            State.CurrentOperation = ImGuizmo::SCALE;

        ImGuizmo::SetOrthographic(Camera.ProjectionType == kraft::CameraProjectionType::Orthographic);
        // ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        float rw = (float)ImGui::GetWindowWidth();
        float rh = (float)ImGui::GetWindowHeight();
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, rw, rh);
        ImGui::GetForegroundDrawList()->AddRect(
            ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + rh),
            ImVec2(ImGui::GetWindowPos().x + rw, ImGui::GetWindowPos().y),
            ImGui::GetColorU32(ImVec4(1, 1, 0, 1))
        );

        // ImGuizmo::Manipulate(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, State.CurrentOperation, State.Mode, Matrix._data, nullptr, State.Snap ? Snap._data : nullptr);
        ImGuizmo::Manipulate(
            Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, State.CurrentOperation, State.Mode, SelectedEntity.ModelMatrix._data
        );
        // ImGuizmo::DrawCubes(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, SelectedEntity.ModelMatrix._data, 1);
        // ImGuizmo::DrawCubes(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, SelectedEntity.ModelMatrix._data, 1);

        if (ImGuizmo::IsUsing())
        {
            ImGuizmo::DecomposeMatrixToComponents(SelectedEntity.ModelMatrix._data, Translation._data, Rotation._data, Scale._data);

            Rotation.x = kraft::DegToRadians(Rotation.x);
            Rotation.y = kraft::DegToRadians(Rotation.y);
            Rotation.z = kraft::DegToRadians(Rotation.z);
            // SelectedEntity.SetTransform();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();

    TestSceneState->GetSelectedEntity().SetTransform(Translation, Rotation, Scale);

    static bool ShowDemoWindow = true;
    ImGui::ShowDemoWindow(&ShowDemoWindow);
    PipelineDebugger();
}

void PipelineDebugger()
{
    using namespace kraft::renderer;
    SimpleObjectState&   Entity = TestSceneState->GetSelectedEntity();
    kraft::Material*     MaterialInstance = Entity.MaterialInstance;
    kraft::Shader*       ShaderInstance = MaterialInstance->Shader;
    kraft::ShaderEffect& Effect = ShaderInstance->ShaderEffect;
    bool                 Recreate = false;

    ImGui::Begin("Pipeline Debugger");
    ImGui::Text(*Effect.Name);
    ImGui::Separator();
    for (int i = 0; i < Effect.RenderStates.Length; i++)
    {
        auto& RenderState = Effect.RenderStates[i];

        ImGui::Text("Blending");
        if (ImGui::BeginCombo("SrcColorBlendFactor", BlendFactor::Strings[RenderState.BlendMode.SrcColorBlendFactor]))
        {
            for (int n = 0; n < BlendFactor::Count; n++)
            {
                const bool IsSelected = (RenderState.BlendMode.SrcColorBlendFactor == n);
                if (ImGui::Selectable(BlendFactor::Strings[n], IsSelected))
                {
                    RenderState.BlendMode.SrcColorBlendFactor = (BlendFactor::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("DstColorBlendFactor", BlendFactor::Strings[RenderState.BlendMode.DstColorBlendFactor]))
        {
            for (int n = 0; n < BlendFactor::Count; n++)
            {
                const bool IsSelected = (RenderState.BlendMode.DstColorBlendFactor == n);
                if (ImGui::Selectable(BlendFactor::Strings[n], IsSelected))
                {
                    RenderState.BlendMode.DstColorBlendFactor = (BlendFactor::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("ColorBlendOperation", BlendFactor::Strings[RenderState.BlendMode.ColorBlendOperation]))
        {
            for (int n = 0; n < BlendOp::Count; n++)
            {
                const bool IsSelected = (RenderState.BlendMode.ColorBlendOperation == n);
                if (ImGui::Selectable(BlendOp::Strings[n], IsSelected))
                {
                    RenderState.BlendMode.ColorBlendOperation = (BlendOp::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("SrcAlphaBlendFactor", BlendFactor::Strings[RenderState.BlendMode.SrcAlphaBlendFactor]))
        {
            for (int n = 0; n < BlendFactor::Count; n++)
            {
                const bool IsSelected = (RenderState.BlendMode.SrcAlphaBlendFactor == n);
                if (ImGui::Selectable(BlendFactor::Strings[n], IsSelected))
                {
                    RenderState.BlendMode.SrcAlphaBlendFactor = (BlendFactor::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("DstAlphaBlendFactor", BlendFactor::Strings[RenderState.BlendMode.DstAlphaBlendFactor]))
        {
            for (int n = 0; n < BlendFactor::Count; n++)
            {
                const bool IsSelected = (RenderState.BlendMode.DstAlphaBlendFactor == n);
                if (ImGui::Selectable(BlendFactor::Strings[n], IsSelected))
                {
                    RenderState.BlendMode.DstAlphaBlendFactor = (BlendFactor::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("AlphaBlendOperation", BlendFactor::Strings[RenderState.BlendMode.AlphaBlendOperation]))
        {
            for (int n = 0; n < BlendOp::Count; n++)
            {
                const bool IsSelected = (RenderState.BlendMode.AlphaBlendOperation == n);
                if (ImGui::Selectable(BlendOp::Strings[n], IsSelected))
                {
                    RenderState.BlendMode.AlphaBlendOperation = (BlendOp::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        if (ImGui::BeginCombo("CullMode", CullModeFlags::Strings[RenderState.CullMode]))
        {
            for (int n = 0; n < CullModeFlags::Count; n++)
            {
                const bool IsSelected = (RenderState.CullMode == n);
                if (ImGui::Selectable(CullModeFlags::Strings[n], IsSelected))
                {
                    RenderState.CullMode = (CullModeFlags::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::DragFloat("Line Width", &RenderState.LineWidth))
        {
            Recreate = true;
        }

        if (ImGui::BeginCombo("PolygonMode", PolygonMode::Strings[RenderState.PolygonMode]))
        {
            for (int n = 0; n < PolygonMode::Count; n++)
            {
                const bool IsSelected = (RenderState.PolygonMode == n);
                if (ImGui::Selectable(PolygonMode::Strings[n], IsSelected))
                {
                    RenderState.PolygonMode = (PolygonMode::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::Text("ZTest");
        ImGui::Checkbox("ZWriteEnable", &RenderState.ZWriteEnable);
        if (ImGui::BeginCombo("ZTestOperation", CompareOp::Strings[RenderState.ZTestOperation]))
        {
            for (int n = 0; n < CompareOp::Count; n++)
            {
                const bool IsSelected = (RenderState.ZTestOperation == n);
                if (ImGui::Selectable(CompareOp::Strings[n], IsSelected))
                {
                    RenderState.ZTestOperation = (CompareOp::Enum)n;
                    Recreate = true;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (Recreate)
    {
        // Can't destroy the pipeline in this frame
        // Renderer->DestroyRenderPipeline(ShaderInstance);
        Renderer->CreateRenderPipeline(ShaderInstance, 0);
    }

    ImGui::End();
}