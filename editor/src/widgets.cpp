#include "widgets.h"

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_engine.h>
#include <math/kraft_math.h>
#include <renderer/kraft_renderer_frontend.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_texture_system.h>
#include <world/kraft_entity.h>

#include "editor.h"
#include "imgui/imgui_renderer.h"
#include "utils.h"

#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <renderer/kraft_resource_manager.h>

#include "imgui/extensions/imguizmo/ImGuizmo.h"
#include <imgui/imgui.h>

struct GizmosState
{
    ImGuizmo::OPERATION CurrentOperation;
    ImGuizmo::MODE      Mode;
    bool                Snap;
} State = {};

static kraft::renderer::Handle<kraft::Texture> SceneTexture;
static ImTextureID                             ImSceneTexture;

void InitImguiWidgets()
{
    GlobalAppState.ImGuiRenderer->AddWidget("Debug", DrawImGuiWidgets);

    SceneTexture = kraft::Renderer->GetSceneViewTexture();
    ImSceneTexture = GlobalAppState.ImGuiRenderer->AddTexture(SceneTexture);

    State.CurrentOperation = ImGuizmo::TRANSLATE;
    State.Mode = ImGuizmo::LOCAL;
    State.Snap = false;
}

void DrawImGuiWidgets(bool refresh)
{
    kraft::Camera& Camera = EditorState::Ptr->CurrentWorld->Camera;

    ImGui::Begin("Debug");

    static float left = -(float)kraft::Platform::GetWindow().Width * 0.5f, right = (float)kraft::Platform::GetWindow().Width * 0.5f,
                 top = -(float)kraft::Platform::GetWindow().Height * 0.5f, bottom = (float)kraft::Platform::GetWindow().Height * 0.5f,
                 nearClip = -1.f, farClip = 1.f;

    static float fov = 45.f;
    static float width = (float)kraft::Platform::GetWindow().Width;
    static float height = (float)kraft::Platform::GetWindow().Height;
    static float nearClipP = 0.1f;
    static float farClipP = 1000.f;

    // To preserve the aspect ratio of the texture
    // kraft::renderer::Handle<kraft::Texture> Resource =
    //     EditorState::Ptr->GetSelectedEntity().MaterialInstance->GetUniform<kraft::renderer::Handle<kraft::Texture>>("DiffuseSampler");
    // kraft::Texture* Texture = kraft::renderer::ResourceManager::Get()->GetTextureMetadata(Resource);

    // kraft::Vec2f ratio = { (float)Texture->Width / kraft::Platform::GetWindow().Width,
    //                        (float)Texture->Height / kraft::Platform::GetWindow().Height };
    // float        downScale = kraft::math::Max(ratio.x, ratio.y);

    static kraft::Vec3f rotationDeg = kraft::Vec3fZero;
    static bool         usePerspectiveProjection = Camera.ProjectionType == kraft::CameraProjectionType::Perspective;

    auto&        Transform = EditorState::Ptr->GetSelectedEntity().GetComponent<kraft::TransformComponent>();
    kraft::Vec3f Translation = Transform.Position;
    kraft::Vec3f Rotation = Transform.Rotation;
    kraft::Vec3f Scale = Transform.Scale;
    rotationDeg.x = kraft::RadiansToDegrees(Rotation.x);
    rotationDeg.y = kraft::RadiansToDegrees(Rotation.y);
    rotationDeg.z = kraft::RadiansToDegrees(Rotation.z);

    if (ImGui::RadioButton("Perspective Projection", usePerspectiveProjection == true))
    {
        usePerspectiveProjection = true;
        SetProjection(kraft::CameraProjectionType::Perspective);
    }

    if (ImGui::RadioButton("Orthographic Projection", usePerspectiveProjection == false))
    {
        usePerspectiveProjection = false;
        SetProjection(kraft::CameraProjectionType::Orthographic);
    }

    // kraft::MeshComponent Mesh = EditorState::Ptr->GetSelectedEntity().GetComponent<kraft::MeshComponent>();
    // auto                 Instance = Mesh.MaterialInstance;
    // static kraft::Vec4f  DiffuseColor = Instance->GetUniform<kraft::Vec4f>("DiffuseColor");
    // if (ImGui::ColorEdit4("Diffuse Color", DiffuseColor))
    // {
    //     kraft::MaterialSystem::SetProperty(Instance, "DiffuseColor", DiffuseColor);
    // }

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
    // ImGui::GetForegroundDrawList()->AddRect(
    //     ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + ViewPortPanelSize.y),
    //     ImVec2(ImGui::GetWindowPos().x + ViewPortPanelSize.x, ImGui::GetWindowPos().y),
    //     ImGui::GetColorU32(ImVec4(1, 0, 0, 1))
    // );

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
        auto& SelectedEntity = EditorState::Ptr->GetSelectedEntity();
        auto& Transform = SelectedEntity.GetComponent<kraft::TransformComponent>();
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
        // ImGui::GetForegroundDrawList()->AddRect(
        //     ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + rh),
        //     ImVec2(ImGui::GetWindowPos().x + rw, ImGui::GetWindowPos().y),
        //     ImGui::GetColorU32(ImVec4(1, 1, 0, 1))
        // );

        // ImGuizmo::Manipulate(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, State.CurrentOperation, State.Mode, Matrix._data, nullptr, State.Snap ? Snap._data : nullptr);
        kraft::Mat4f EntityWorldTransform = EditorState::Ptr->CurrentWorld->GetWorldSpaceTransformMatrix(SelectedEntity);
        ImGuizmo::Manipulate(
            Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, State.CurrentOperation, State.Mode, EntityWorldTransform
        );
        // ImGuizmo::DrawCubes(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, SelectedEntity.ModelMatrix._data, 1);
        // ImGuizmo::DrawCubes(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, SelectedEntity.ModelMatrix._data, 1);

        if (ImGuizmo::IsUsing())
        {
            // ImGuizmo::DecomposeMatrixToComponents(Transform.ModelMatrix._data, Translation._data, Rotation._data, Scale._data);
            kraft::Entity ParentEntity = EditorState::Ptr->CurrentWorld->GetEntity(SelectedEntity.GetParent());
            kraft::Mat4f  EntityParentWorldTransform = EditorState::Ptr->CurrentWorld->GetWorldSpaceTransformMatrix(ParentEntity);
            kraft::Mat4f  EntityLocalTransform = EntityWorldTransform * kraft::Inverse(EntityParentWorldTransform);

            EntityLocalTransform.Decompose(Translation, Rotation, Scale);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();

    Transform.SetTransform(Translation, Rotation, Scale);

    static bool ShowDemoWindow = true;
    ImGui::ShowDemoWindow(&ShowDemoWindow);
    PipelineDebugger();
    HierarchyPanel();
}

void PipelineDebugger()
{
    using namespace kraft::renderer;
    auto& SelectedEntity = EditorState::Ptr->GetSelectedEntity();
    if (!SelectedEntity.HasComponent<kraft::MeshComponent>())
        return;

    auto&                Mesh = SelectedEntity.GetComponent<kraft::MeshComponent>();
    kraft::Material*     MaterialInstance = Mesh.MaterialInstance;
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

static void DrawNodeHierarchy(const kraft::Entity& Root)
{
    auto&              Metadata = Root.GetComponent<kraft::MetadataComponent>();
    auto&              Children = Root.GetChildren();
    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
    if (Children.Length == 0)
        Flags |= ImGuiTreeNodeFlags_Leaf;

    if (EditorState::Ptr->SelectedEntity == Root.EntityHandle)
        Flags |= ImGuiTreeNodeFlags_Selected;

    if (ImGui::TreeNodeEx(*Metadata.Name, Flags))
    {
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            EditorState::Ptr->SelectedEntity = Root.EntityHandle;
            KDEBUG("%s clicked", *Metadata.Name);
        }

        for (int i = 0; i < Children.Length; i++)
        {
            DrawNodeHierarchy(EditorState::Ptr->CurrentWorld->GetEntity(Children[i]));
        }

        ImGui::TreePop();
    }
}

void HierarchyPanel()
{
    using namespace kraft;
    if (!EditorState::Ptr->CurrentWorld)
        return;

    ImGui::Begin("World Outline");
    const Entity& Root = EditorState::Ptr->CurrentWorld->GetRoot();
    DrawNodeHierarchy(Root);
    ImGui::End();

#if 0
    if (ImGui::TreeNode("World"))
    {
        const Entity& Root = EditorState::Ptr->CurrentWorld->GetRoot();
        if (ImGui::TreeNode("Advanced, with Selectable nodes"))
        {
            static ImGuiTreeNodeFlags base_flags =
                ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            static bool align_label_with_current_x_position = false;
            static bool test_drag_and_drop = false;
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_OpenOnArrow", &base_flags, ImGuiTreeNodeFlags_OpenOnArrow);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_OpenOnDoubleClick", &base_flags, ImGuiTreeNodeFlags_OpenOnDoubleClick);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_SpanAvailWidth", &base_flags, ImGuiTreeNodeFlags_SpanAvailWidth);
            ImGui::SameLine();
            HelpMarker("Extend hit area to all available width instead of allowing more items to be laid out after the node.");
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_SpanFullWidth", &base_flags, ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_SpanTextWidth", &base_flags, ImGuiTreeNodeFlags_SpanTextWidth);
            ImGui::SameLine();
            HelpMarker("Reduce hit area to the text label and a bit of margin.");
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_SpanAllColumns", &base_flags, ImGuiTreeNodeFlags_SpanAllColumns);
            ImGui::SameLine();
            HelpMarker("For use in Tables only.");
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_AllowOverlap", &base_flags, ImGuiTreeNodeFlags_AllowOverlap);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_Framed", &base_flags, ImGuiTreeNodeFlags_Framed);
            ImGui::SameLine();
            HelpMarker("Draw frame with background (e.g. for CollapsingHeader)");
            ImGui::Checkbox("Align label with current X position", &align_label_with_current_x_position);
            ImGui::Checkbox("Test tree node as drag source", &test_drag_and_drop);
            ImGui::Text("Hello!");
            if (align_label_with_current_x_position)
                ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

            // 'selection_mask' is dumb representation of what may be user-side selection state.
            //  You may retain selection state inside or outside your objects in whatever format you see fit.
            // 'node_clicked' is temporary storage of what node we have clicked to process selection at the end
            /// of the loop. May be a pointer to your own node type, etc.
            static int selection_mask = (1 << 2);
            int        node_clicked = -1;
            for (int i = 0; i < 6; i++)
            {
                // Disable the default "open on single-click behavior" + set Selected flag according to our selection.
                // To alter selection we use IsItemClicked() && !IsItemToggledOpen(), so clicking on an arrow doesn't alter selection.
                ImGuiTreeNodeFlags node_flags = base_flags;
                const bool         is_selected = (selection_mask & (1 << i)) != 0;
                if (is_selected)
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                if (i < 3)
                {
                    // Items 0..2 are Tree Node
                    bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, "Selectable Node %d", i);
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        node_clicked = i;
                    if (test_drag_and_drop && ImGui::BeginDragDropSource())
                    {
                        ImGui::SetDragDropPayload("_TREENODE", NULL, 0);
                        ImGui::Text("This is a drag and drop source");
                        ImGui::EndDragDropSource();
                    }
                    if (i == 2)
                    {
                        // Item 2 has an additional inline button to help demonstrate SpanTextWidth.
                        ImGui::SameLine();
                        if (ImGui::SmallButton("button"))
                        {
                        }
                    }
                    if (node_open)
                    {
                        ImGui::BulletText("Blah blah\nBlah Blah");
                        ImGui::TreePop();
                    }
                }
                else
                {
                    // Items 3..5 are Tree Leaves
                    // The only reason we use TreeNode at all is to allow selection of the leaf. Otherwise we can
                    // use BulletText() or advance the cursor by GetTreeNodeToLabelSpacing() and call Text().
                    node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // ImGuiTreeNodeFlags_Bullet
                    ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, "Selectable Leaf %d", i);
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        node_clicked = i;
                    if (test_drag_and_drop && ImGui::BeginDragDropSource())
                    {
                        ImGui::SetDragDropPayload("_TREENODE", NULL, 0);
                        ImGui::Text("This is a drag and drop source");
                        ImGui::EndDragDropSource();
                    }
                }
            }
            if (node_clicked != -1)
            {
                // Update selection state
                // (process outside of tree loop to avoid visual inconsistencies during the clicking frame)
                if (ImGui::GetIO().KeyCtrl)
                    selection_mask ^= (1 << node_clicked); // CTRL+click to toggle
                else //if (!(selection_mask & (1 << node_clicked))) // Depending on selection behavior you want, may want to preserve selection when clicking on item that is part of the selection
                    selection_mask = (1 << node_clicked); // Click to single-select
            }
            if (align_label_with_current_x_position)
                ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}