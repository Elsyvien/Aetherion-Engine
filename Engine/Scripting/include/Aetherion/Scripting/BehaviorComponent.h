#pragma once

#include "Aetherion/Scene/Component.h"
#include "Aetherion/Scene/TransformComponent.h"
#include <memory>

namespace Aetherion::Scripting
{

/// @brief Base class for runtime-compiled behavior components
/// Generated behaviors should inherit from this class
class BehaviorComponent : public Scene::Component
{
public:
    BehaviorComponent() = default;
    virtual ~BehaviorComponent() = default;

    [[nodiscard]] std::string GetDisplayName() const override
    {
        return "BehaviorComponent";
    }

protected:
    /// @brief Helper to get transform component
    std::shared_ptr<Scene::TransformComponent> GetTransform() const
    {
        if (!GetEntity())
            return nullptr;
        return GetEntity()->GetComponent<Scene::TransformComponent>();
    }
};

} // namespace Aetherion::Scripting
