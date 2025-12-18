#pragma once

#include <QObject>
#include <memory>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Entity;
class Scene;
} // namespace Aetherion::Scene

namespace Aetherion::Editor
{
class EditorSelection : public QObject
{
    Q_OBJECT

public:
    explicit EditorSelection(QObject* parent = nullptr);
    ~EditorSelection() override = default;

    void SetActiveScene(std::shared_ptr<Scene::Scene> scene);
    void SelectEntityById(Core::EntityId id);
    void SelectEntity(std::shared_ptr<Scene::Entity> entity);
    void Clear();

    [[nodiscard]] std::shared_ptr<Scene::Entity> GetSelectedEntity() const noexcept;

signals:
    void SelectionChanged(Aetherion::Core::EntityId id);
    void SelectionCleared();

private:
    std::shared_ptr<Scene::Scene> m_scene;
    std::shared_ptr<Scene::Entity> m_selectedEntity;
};
} // namespace Aetherion::Editor
