#pragma once

#include "Aetherion/Scene/System.h"
#include <memory>

namespace Aetherion::Assets
{
class AssetRegistry;
}

namespace Aetherion::Scene
{

class AnimationSystem : public System
{
public:
    AnimationSystem() = default;
    ~AnimationSystem() override = default;

    [[nodiscard]] std::string GetName() const override { return "AnimationSystem"; }
    void Configure(Runtime::EngineContext& context) override;
    void Update(Scene& scene, float deltaTime) override;

    // Debug visualization
    void SetDebugDrawEnabled(bool enabled) { m_debugDraw = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const noexcept { return m_debugDraw; }

    // Performance stats
    [[nodiscard]] size_t GetActiveAnimatorCount() const noexcept { return m_activeAnimatorCount; }
    [[nodiscard]] size_t GetTotalBonesProcessed() const noexcept { return m_totalBonesProcessed; }

private:
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    bool m_debugDraw{false};

    // Stats
    size_t m_activeAnimatorCount{0};
    size_t m_totalBonesProcessed{0};
};

} // namespace Aetherion::Scene
