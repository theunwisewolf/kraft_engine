#include "editor.h"

#include "world/kraft_entity.h"
#include <renderer/kraft_renderer_frontend.h>
#include <core/kraft_engine.h>

EditorState* EditorState::Ptr = nullptr;

EditorState::EditorState()
{
    EditorState::Ptr = this;
    this->RenderSurface = kraft::Engine::Renderer.CreateRenderSurface("SceneView", 200.0f, 200.0f, true);
}

kraft::Entity EditorState::GetSelectedEntity()
{
    return this->CurrentWorld->GetEntity(SelectedEntity);
}
