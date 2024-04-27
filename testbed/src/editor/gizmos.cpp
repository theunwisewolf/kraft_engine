#include "gizmos.h"

#include "imgui/imgui.h"
#include "imgui/extensions/imguizmo/ImGuizmo.h"

struct GizmosState
{
    ImGuizmo::OPERATION CurrentOperation;
    ImGuizmo::MODE      Mode;
    bool                Snap;
} State = {};

void InitGizmos()
{
    State.CurrentOperation = ImGuizmo::TRANSLATE;
    State.Mode = ImGuizmo::LOCAL;
    State.Snap = false;
}

void EditTransform(const kraft::Camera& Camera, kraft::Mat4f& Matrix)
{
    if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Q))
        State.CurrentOperation = ImGuizmo::TRANSLATE;

    if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_W))
        State.CurrentOperation = ImGuizmo::ROTATE;

    if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_E))
        State.CurrentOperation = ImGuizmo::SCALE;

    if (ImGui::RadioButton("Translate", State.CurrentOperation == ImGuizmo::TRANSLATE))
        State.CurrentOperation = ImGuizmo::TRANSLATE;

    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", State.CurrentOperation == ImGuizmo::ROTATE))
        State.CurrentOperation = ImGuizmo::ROTATE;

    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", State.CurrentOperation == ImGuizmo::SCALE))
        State.CurrentOperation = ImGuizmo::SCALE;

    float Translation[3], Rotation[3], Scale[3];
    ImGuizmo::DecomposeMatrixToComponents(Matrix._data, Translation, Rotation, Scale);
    ImGui::InputFloat3("Tr", Translation);
    ImGui::InputFloat3("Rt", Rotation);
    ImGui::InputFloat3("Sc", Scale);

    // ImGuizmo::RecomposeMatrixFromComponents(Translation, Rotation, Scale, Matrix._data);

    if (State.CurrentOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", State.Mode == ImGuizmo::LOCAL))
            State.Mode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", State.Mode == ImGuizmo::WORLD))
            State.Mode = ImGuizmo::WORLD;
    }
    
    if (ImGui::IsKeyPressed(ImGuiKey::ImGuiMod_Ctrl))
        State.Snap = !State.Snap;

    ImGui::Checkbox("Snap", &State.Snap);
    ImGui::SameLine();

    kraft::Vec3f Snap;
    switch (State.CurrentOperation)
    {
    case ImGuizmo::TRANSLATE:
        Snap = kraft::Vec3fOne;
        ImGui::InputFloat3("Snap", Snap._data);
        break;
    case ImGuizmo::ROTATE:
        Snap = kraft::Vec3fOne;
        ImGui::InputFloat("Angle Snap", Snap._data);
        break;
    case ImGuizmo::SCALE:
        Snap = kraft::Vec3fOne;
        ImGui::InputFloat("Scale Snap", Snap._data);
        break;
    }

    ImGuiIO& io = ImGui::GetIO();
    
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    // ImGuizmo::Manipulate(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, State.CurrentOperation, State.Mode, Matrix._data, nullptr, State.Snap ? Snap._data : nullptr);
    ImGuizmo::Manipulate(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, State.CurrentOperation, State.Mode, Matrix._data);
    ImGuizmo::DrawCubes(Camera.ViewMatrix._data, Camera.ProjectionMatrix._data, Matrix._data, 1);
}