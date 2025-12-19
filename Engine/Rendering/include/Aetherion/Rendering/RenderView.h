#pragma once

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
    float model[16]{};
    bool hasModel{false};
};

struct RenderBatch
{
    std::vector<RenderInstance> instances;
};

struct RenderView
{
    std::vector<RenderInstance> instances;
    std::vector<RenderBatch> batches;
    std::unordered_map<Core::EntityId, const Scene::TransformComponent*> transforms;
    std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent*> meshes;
};
} // namespace Aetherion::Rendering
