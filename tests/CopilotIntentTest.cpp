#include <QCoreApplication>

#include "Aetherion/Editor/AICopilotProcessor.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

using namespace Aetherion;

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    auto scene = std::make_shared<Scene::Scene>("TestScene");
    auto entity = scene->CreateEntity("Mover");
    entity->AddComponent(std::make_shared<Scene::TransformComponent>());

    Editor::AICopilotProcessor processor(nullptr);
    processor.SetScene(scene);
    processor.SetSelectedEntity(entity);

    auto dryRun = processor.ProcessPrompt("move selection up 2 preview");
    if (!dryRun.dryRun || dryRun.previewActions.empty()) {
        return 1;
    }

    auto parentRes = processor.ProcessPrompt("parent selection to 1", false);
    if (parentRes.response.isEmpty()) {
        return 1;
    }

    return 0;
}
