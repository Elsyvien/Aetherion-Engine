#pragma once

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene {
class Entity;
}

namespace Aetherion::Scripting {

/// @brief Represents an instance of a script attached to an entity
class ScriptInstance {
public:
    virtual ~ScriptInstance() = default;

    /// @brief Set the entity this script is attached to
    virtual void SetEntity(Scene::Entity* entity) = 0;

    /// @brief Called when the script is first created or attached
    virtual void OnCreate() = 0;

    /// @brief Called every frame
    virtual void OnUpdate(float deltaTime) = 0;

    /// @brief Called when the script is destroyed or detached
    virtual void OnDestroy() = 0;
};

} // namespace Aetherion::Scripting
