#pragma once

#include <memory>
#include <string>
#include <functional>
#include <QString>

#include <vector>
#include "Aetherion/Core/Types.h"

namespace Aetherion::Assets {
    class AssetRegistry;
}

namespace Aetherion::Scene {
    class Scene;
    class Entity; // Forward declaration for Entity
}

namespace Aetherion::Editor {

class Command;

struct CopilotResult {
    QString response;
    std::vector<Core::EntityId> createdEntityIds;
    std::vector<QString> previewActions;
    bool dryRun{false};
    bool requestFocus{false};
};

class AICopilotProcessor {
public:
    using CommandExecutor = std::function<void(std::unique_ptr<Command>)>;      

    AICopilotProcessor(CommandExecutor executor);

    void SetScene(std::shared_ptr<Scene::Scene> scene);
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);     
    void SetSelectedEntity(std::shared_ptr<Scene::Entity> selected);

    // Processes the prompt and returns a result
    CopilotResult ProcessPrompt(const QString& prompt, bool allowDryRun = true);

private:
    std::shared_ptr<Scene::Scene> m_scene;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    std::shared_ptr<Scene::Entity> m_selectedEntity;
    CommandExecutor m_executor;
};

} // namespace Aetherion::Editor
