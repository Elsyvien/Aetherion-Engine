#include "Aetherion/Scene/AnimationSystem.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/AnimatorComponent.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Assets/AssetRegistry.h"

namespace Aetherion::Scene
{

void AnimationSystem::Configure(Runtime::EngineContext& context)
{
    m_assetRegistry = context.GetAssetRegistry();
}

void AnimationSystem::Update(Scene& scene, float deltaTime)
{
    m_activeAnimatorCount = 0;
    m_totalBonesProcessed = 0;

    const auto& entities = scene.GetEntities();

    for (const auto& entity : entities)
    {
        if (!entity) continue;

        // Check if entity has an Animator component
        auto animator = entity->GetComponent<AnimatorComponent>();
        if (!animator) continue;

        // Check for skeleton component
        auto skeleton = entity->GetComponent<SkeletonComponent>();
        if (!skeleton || !skeleton->GetSkeleton()) continue;

        ++m_activeAnimatorCount;
        m_totalBonesProcessed += skeleton->GetSkeleton()->GetBoneCount();

        // AnimatorComponent::OnUpdate handles the actual animation sampling
        // This is called by the Scene's component update loop
        // Here we just track stats and could add additional system-level processing

        // Apply root motion to entity transform if enabled
        if (animator->IsRootMotionEnabled())
        {
            auto transform = entity->GetComponent<TransformComponent>();
            if (transform)
            {
                auto delta = animator->GetRootMotionDelta();
                const auto& pos = transform->GetPosition();
                transform->SetPosition(pos[0] + delta.x, pos[1] + delta.y, pos[2] + delta.z);
            }
        }
    }
}

} // namespace Aetherion::Scene
