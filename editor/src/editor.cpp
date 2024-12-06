#include "editor.h"

#include "world/kraft_entity.h"

EditorState* EditorState::Ptr = nullptr;

kraft::Entity& EditorState::GetSelectedEntity()
{
    return this->CurrentWorld->GetEntity(SelectedEntity);
}
