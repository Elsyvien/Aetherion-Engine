#include "Aetherion/Editor/EditorSelection.h"

#include <utility>

#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"

namespace Aetherion::Editor
{
EditorSelection::EditorSelection(QObject* parent)
    : QObject(parent)
{
}

void EditorSelection::SetActiveScene(std::shared_ptr<Scene::Scene> scene)
{
    m_scene = std::move(scene);
    if (m_scene && m_selectedEntity)
    {
        if (!m_scene->FindEntityById(m_selectedEntity->GetId()))
        {
            Clear();
        }
    }
}

void EditorSelection::SelectEntityById(Core::EntityId id)
{
    if (!m_scene)
    {
        Clear();
        return;
    }

    auto entity = m_scene->FindEntityById(id);
    SelectEntity(std::move(entity));
}

void EditorSelection::SelectEntity(std::shared_ptr<Scene::Entity> entity)
{
    if (m_selectedEntity == entity)
    {
        return;
    }

    m_selectedEntity = std::move(entity);
    if (m_selectedEntity)
    {
        emit SelectionChanged(m_selectedEntity->GetId());
    }
    else
    {
        emit SelectionCleared();
    }
}

void EditorSelection::Clear()
{
    if (m_selectedEntity)
    {
        m_selectedEntity.reset();
    }
    emit SelectionCleared();
}

std::shared_ptr<Scene::Entity> EditorSelection::GetSelectedEntity() const noexcept
{
    return m_selectedEntity;
}
} // namespace Aetherion::Editor
