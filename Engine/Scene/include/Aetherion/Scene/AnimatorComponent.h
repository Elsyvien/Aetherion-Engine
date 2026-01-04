#pragma once

#include "Aetherion/Scene/Component.h"
#include "Aetherion/Assets/Animation.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace Aetherion::Scene
{

// ============================================================================
// SkeletonComponent - Holds bone hierarchy and skinning data
// ============================================================================

class SkeletonComponent : public Component
{
public:
    SkeletonComponent() = default;
    ~SkeletonComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override { return "Skeleton"; }

    // Skeleton data
    void SetSkeleton(std::shared_ptr<Assets::Skeleton> skeleton);
    [[nodiscard]] std::shared_ptr<Assets::Skeleton> GetSkeleton() const noexcept { return m_skeleton; }

    // Current pose (result of animation sampling)
    [[nodiscard]] Assets::AnimationPose& GetCurrentPose() noexcept { return m_currentPose; }
    [[nodiscard]] const Assets::AnimationPose& GetCurrentPose() const noexcept { return m_currentPose; }

    // Get skinning matrices for shader upload
    [[nodiscard]] const std::vector<Assets::Mat4>& GetSkinningMatrices() const;

    // Update global transforms from current local pose
    void UpdateGlobalTransforms();

    // Reset to bind pose
    void ResetToBindPose();

private:
    std::shared_ptr<Assets::Skeleton> m_skeleton;
    Assets::AnimationPose m_currentPose;
};

// ============================================================================
// AnimatorComponent - Controls animation playback and blending
// ============================================================================

struct AnimationLayer
{
    std::string name{"Base"};
    Assets::AnimationState state;
    float blendWeight{1.0f};
    bool additive{false};

    // Bone mask (empty = all bones)
    std::vector<int> boneMask;
};

class AnimatorComponent : public Component
{
public:
    AnimatorComponent() = default;
    ~AnimatorComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override { return "Animator"; }

    // Animation clips library
    void AddClip(const std::string& name, std::shared_ptr<Assets::AnimationClip> clip);
    void RemoveClip(const std::string& name);
    [[nodiscard]] std::shared_ptr<Assets::AnimationClip> GetClip(const std::string& name) const;
    [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<Assets::AnimationClip>>& GetClips() const noexcept { return m_clips; }

    // Playback control (operates on base layer by default)
    void Play(const std::string& clipName, float blendTime = 0.0f);
    void Stop();
    void Pause();
    void Resume();

    // Crossfade between current animation and new one
    void CrossFade(const std::string& clipName, float fadeTime);

    // Layer management
    size_t AddLayer(const std::string& name);
    [[nodiscard]] AnimationLayer* GetLayer(size_t index);
    [[nodiscard]] const AnimationLayer* GetLayer(size_t index) const;
    [[nodiscard]] AnimationLayer* GetLayer(const std::string& name);
    [[nodiscard]] size_t GetLayerCount() const noexcept { return m_layers.size(); }

    // Play on specific layer
    void PlayOnLayer(size_t layerIndex, const std::string& clipName, float blendTime = 0.0f);

    // Parameters for blend trees
    void SetFloat(const std::string& name, float value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);
    [[nodiscard]] float GetFloat(const std::string& name) const;
    [[nodiscard]] bool GetBool(const std::string& name) const;

    // Global playback speed
    void SetSpeed(float speed) { m_globalSpeed = speed; }
    [[nodiscard]] float GetSpeed() const noexcept { return m_globalSpeed; }

    // Root motion
    void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }
    [[nodiscard]] bool IsRootMotionEnabled() const noexcept { return m_rootMotionEnabled; }
    [[nodiscard]] Assets::Vec3 GetRootMotionDelta() const noexcept { return m_rootMotionDelta; }
    [[nodiscard]] Assets::Quaternion GetRootRotationDelta() const noexcept { return m_rootRotationDelta; }

    // Events
    using AnimationEventCallback = std::function<void(const std::string& eventName)>;
    void SetEventCallback(AnimationEventCallback callback) { m_eventCallback = std::move(callback); }

    // Current state info
    [[nodiscard]] bool IsPlaying() const;
    [[nodiscard]] float GetCurrentTime() const;
    [[nodiscard]] float GetNormalizedTime() const;
    [[nodiscard]] std::string GetCurrentClipName() const;

protected:
    void OnUpdate(float deltaTime) override;

private:
    void SamplePose(float deltaTime);
    void ApplyLayerBlending();
    void ProcessRootMotion(const Assets::AnimationPose& previousPose);

    // Animation clips
    std::unordered_map<std::string, std::shared_ptr<Assets::AnimationClip>> m_clips;

    // Animation layers (layer 0 is base layer)
    std::vector<AnimationLayer> m_layers;

    // Blend parameters
    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, bool> m_boolParams;
    std::vector<std::string> m_pendingTriggers;

    // Crossfade state
    struct CrossFadeState
    {
        std::string fromClip;
        std::string toClip;
        float fadeTime{0.0f};
        float elapsed{0.0f};
        bool active{false};
    } m_crossFade;

    // Playback settings
    float m_globalSpeed{1.0f};
    bool m_rootMotionEnabled{false};

    // Root motion delta (applied to transform)
    Assets::Vec3 m_rootMotionDelta{0.0f, 0.0f, 0.0f};
    Assets::Quaternion m_rootRotationDelta{Assets::Quaternion::Identity()};

    // Event callback
    AnimationEventCallback m_eventCallback;

    // Cached skeleton reference
    SkeletonComponent* m_cachedSkeleton{nullptr};
};

} // namespace Aetherion::Scene
