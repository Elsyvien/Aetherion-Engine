#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class TransformComponent;
class MeshRendererComponent;
} // namespace Aetherion::Scene

namespace Aetherion::Rendering
{
struct RenderInstance
{
    Core::EntityId entityId{0};
    const Scene::TransformComponent* transform{nullptr};
    const Scene::MeshRendererComponent* mesh{nullptr};
    std::string meshAssetId;
    std::string albedoTextureId;
    float model[16]{};
    bool hasModel{false};
};

struct RenderBatch
{
    std::vector<RenderInstance> instances;
};

struct RenderDirectionalLight
{
    bool enabled{false};
    float direction[3]{0.0f, -1.0f, 0.0f};
    float color[3]{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float ambientColor[3]{0.18f, 0.18f, 0.20f};
    // For gizmo rendering
    float position[3]{0.0f, 3.0f, 0.0f};
    Core::EntityId entityId{0};
};

struct RenderView
{
    std::vector<RenderInstance> instances;
    std::vector<RenderBatch> batches;
    std::unordered_map<Core::EntityId, const Scene::TransformComponent*> transforms;
    std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent*> meshes;
    Core::EntityId selectedEntityId{0};
    RenderDirectionalLight directionalLight{};
};
} // namespace Aetherion::Rendering
