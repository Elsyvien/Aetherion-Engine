#pragma once

#include "Aetherion/Scripting/BehaviorComponent.h"
#include "Aetherion/Scripting/GameModule.h"
#include <string>

namespace Aetherion::Generated
{

/// @brief Simple rotation behavior
/// Makes an entity rotate continuously
class RotationBehavior : public Scripting::BehaviorComponent
{
public:
    RotationBehavior() = default;
    ~RotationBehavior() override = default;

    [[nodiscard]] std::string GetDisplayName() const override { return "Rotation Behavior"; }

    void SetRotationSpeed(float speed) { m_rotationSpeed = speed; }
    [[nodiscard]] float GetRotationSpeed() const { return m_rotationSpeed; }

protected:
    void OnAdded() override;
    void OnUpdate(float deltaTime) override;
    void OnRemoved() override;

private:
    float m_rotationSpeed{45.0f}; // degrees per second
};

// Module implementation for hot-reload
class RotationBehaviorModule : public Scripting::IGameModule
{
public:
    void OnLoad(Scene::Scene* scene) override;
    void OnUnload() override;
    const char* GetModuleName() const override { return "RotationBehavior"; }
    uint32_t GetModuleVersion() const override { return 1; }

private:
    Scene::Scene* m_scene{nullptr};
};

} // namespace Aetherion::Generated

// Export module entry points
extern "C" {
    AETHERION_EXPORT Aetherion::Scripting::IGameModule* CreateGameModule();
    AETHERION_EXPORT void DestroyGameModule(Aetherion::Scripting::IGameModule* module);
}
