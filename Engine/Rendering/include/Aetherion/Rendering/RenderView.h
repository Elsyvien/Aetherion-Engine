#pragma once

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
};

struct RenderView
{
    std::vector<RenderInstance> instances;
};
} // namespace Aetherion::Rendering
