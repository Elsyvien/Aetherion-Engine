#include "Aetherion/Scene/AnimatorComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Assets/AnimationLoader.h"
#include <algorithm>
#include <filesystem>

namespace Aetherion::Scene
{

// ============================================================================
// SkeletonComponent Implementation
// ============================================================================

void SkeletonComponent::SetSkeleton(std::shared_ptr<Assets::Skeleton> skeleton)
{
    m_skeleton = std::move(skeleton);
    m_skeletonAssetPath.clear();
    if (m_skeleton)
    {
        m_currentPose.Resize(m_skeleton->GetBoneCount());
        ResetToBindPose();
    }
    else
    {
        m_currentPose.Resize(0);
    }

    if (m_skeleton && GetEntity())
    {
        if (auto animator = GetEntity()->GetComponent<AnimatorComponent>())
        {
            for (const auto& [name, clip] : animator->GetClips())
            {
                if (clip)
                {
                    clip->BindToSkeleton(*m_skeleton);
                }
            }
        }
    }
}

bool SkeletonComponent::LoadSkeletonFromFile(const std::string& path)
{
    if (path.empty()) return false;

    auto skeleton = Assets::AnimationLoader::LoadSkeleton(path);
    if (!skeleton) return false;

    SetSkeleton(skeleton);
    m_skeletonAssetPath = path;
    return true;
}

const std::vector<Assets::Mat4>& SkeletonComponent::GetSkinningMatrices() const
{
    return m_currentPose.GetSkinningMatrices();
}

void SkeletonComponent::UpdateGlobalTransforms()
{
    if (m_skeleton)
    {
        m_currentPose.ComputeGlobalTransforms(*m_skeleton);
    }
}

void SkeletonComponent::ResetToBindPose()
{
    if (!m_skeleton) return;

    const auto& bones = m_skeleton->GetBones();
    for (size_t i = 0; i < bones.size(); ++i)
    {
        auto& local = m_currentPose.GetLocalTransform(i);
        local.position = bones[i].restPosition;
        local.rotation = bones[i].restRotation;
        local.scale = bones[i].restScale;
    }

    UpdateGlobalTransforms();
}

// ============================================================================
// AnimatorComponent Implementation
// ============================================================================

void AnimatorComponent::AddClip(const std::string& name, std::shared_ptr<Assets::AnimationClip> clip)
{
    m_clipSources.erase(name);
    if (clip)
    {
        if (!m_cachedSkeleton && GetEntity())
        {
            m_cachedSkeleton = GetEntity()->GetComponent<SkeletonComponent>().get();
        }

        if (m_cachedSkeleton && m_cachedSkeleton->GetSkeleton())
        {
            clip->BindToSkeleton(*m_cachedSkeleton->GetSkeleton());
        }
    }
    m_clips[name] = std::move(clip);
}

bool AnimatorComponent::AddClipFromFile(const std::string& name, const std::string& path)
{
    if (path.empty()) return false;

    auto clip = Assets::AnimationLoader::LoadAnimation(path);
    if (!clip) return false;

    std::string clipName = name;
    if (clipName.empty())
    {
        clipName = clip->GetName();
    }
    if (clipName.empty())
    {
        std::filesystem::path clipPath(path);
        clipName = clipPath.stem().string();
    }

    clip->SetName(clipName);
    AddClip(clipName, clip);
    m_clipSources[clipName] = path;
    return true;
}

void AnimatorComponent::RemoveClip(const std::string& name)
{
    m_clips.erase(name);
    m_clipSources.erase(name);
}

std::shared_ptr<Assets::AnimationClip> AnimatorComponent::GetClip(const std::string& name) const
{
    auto it = m_clips.find(name);
    return it != m_clips.end() ? it->second : nullptr;
}

void AnimatorComponent::Play(const std::string& clipName, float blendTime)
{
    if (m_layers.empty())
    {
        AddLayer("Base");
    }

    PlayOnLayer(0, clipName, blendTime);
}

void AnimatorComponent::Stop()
{
    for (auto& layer : m_layers)
    {
        layer.state.Stop();
    }
    m_crossFade.active = false;
}

void AnimatorComponent::Pause()
{
    for (auto& layer : m_layers)
    {
        layer.state.Pause();
    }
}

void AnimatorComponent::Resume()
{
    for (auto& layer : m_layers)
    {
        layer.state.Resume();
    }
}

void AnimatorComponent::CrossFade(const std::string& clipName, float fadeTime)
{
    if (m_layers.empty() || !m_layers[0].state.playing)
    {
        Play(clipName, 0.0f);
        return;
    }

    auto clip = GetClip(clipName);
    if (!clip) return;

    m_crossFade.fromClip = GetCurrentClipName();
    m_crossFade.toClip = clipName;
    m_crossFade.fadeTime = fadeTime;
    m_crossFade.elapsed = 0.0f;
    m_crossFade.active = true;

    // Prepare the new clip state (will be blended in)
    // Store on layer for actual transition
}

size_t AnimatorComponent::AddLayer(const std::string& name)
{
    AnimationLayer layer;
    layer.name = name;
    m_layers.push_back(layer);
    return m_layers.size() - 1;
}

AnimationLayer* AnimatorComponent::GetLayer(size_t index)
{
    if (index >= m_layers.size()) return nullptr;
    return &m_layers[index];
}

const AnimationLayer* AnimatorComponent::GetLayer(size_t index) const
{
    if (index >= m_layers.size()) return nullptr;
    return &m_layers[index];
}

AnimationLayer* AnimatorComponent::GetLayer(const std::string& name)
{
    for (auto& layer : m_layers)
    {
        if (layer.name == name) return &layer;
    }
    return nullptr;
}

void AnimatorComponent::PlayOnLayer(size_t layerIndex, const std::string& clipName, float blendTime)
{
    if (layerIndex >= m_layers.size()) return;

    auto clip = GetClip(clipName);
    if (!clip) return;

    auto& layer = m_layers[layerIndex];
    layer.state.clip = clip;
    layer.state.time = 0.0f;
    layer.state.Play();

    if (blendTime > 0.0f)
    {
        // TODO: Implement blend-in from current pose
        layer.blendWeight = 0.0f;
    }
    else
    {
        layer.blendWeight = 1.0f;
    }
}

void AnimatorComponent::SetFloat(const std::string& name, float value)
{
    m_floatParams[name] = value;
}

void AnimatorComponent::SetBool(const std::string& name, bool value)
{
    m_boolParams[name] = value;
}

void AnimatorComponent::SetTrigger(const std::string& name)
{
    m_pendingTriggers.push_back(name);
}

float AnimatorComponent::GetFloat(const std::string& name) const
{
    auto it = m_floatParams.find(name);
    return it != m_floatParams.end() ? it->second : 0.0f;
}

bool AnimatorComponent::GetBool(const std::string& name) const
{
    auto it = m_boolParams.find(name);
    return it != m_boolParams.end() ? it->second : false;
}

bool AnimatorComponent::IsPlaying() const
{
    if (m_layers.empty()) return false;
    return m_layers[0].state.playing;
}

float AnimatorComponent::GetCurrentTime() const
{
    if (m_layers.empty()) return 0.0f;
    return m_layers[0].state.time;
}

float AnimatorComponent::GetNormalizedTime() const
{
    if (m_layers.empty() || !m_layers[0].state.clip) return 0.0f;
    float duration = m_layers[0].state.clip->GetDuration();
    return duration > 0.0f ? m_layers[0].state.time / duration : 0.0f;
}

std::string AnimatorComponent::GetCurrentClipName() const
{
    if (m_layers.empty() || !m_layers[0].state.clip) return "";

    for (const auto& [name, clip] : m_clips)
    {
        if (clip == m_layers[0].state.clip) return name;
    }
    return "";
}

void AnimatorComponent::OnUpdate(float deltaTime)
{
    // Cache skeleton component reference
    if (!m_cachedSkeleton && GetEntity())
    {
        m_cachedSkeleton = GetEntity()->GetComponent<SkeletonComponent>().get();

        // Rebind all clips to skeleton
        if (m_cachedSkeleton && m_cachedSkeleton->GetSkeleton())
        {
            for (auto& [name, clip] : m_clips)
            {
                if (clip)
                {
                    clip->BindToSkeleton(*m_cachedSkeleton->GetSkeleton());
                }
            }
        }
    }

    if (!m_cachedSkeleton) return;

    // Store previous pose for root motion
    Assets::AnimationPose previousPose = m_cachedSkeleton->GetCurrentPose();

    // Sample and blend animations
    SamplePose(deltaTime * m_globalSpeed);

    // Process root motion
    if (m_rootMotionEnabled)
    {
        ProcessRootMotion(previousPose);
    }

    // Clear triggers
    m_pendingTriggers.clear();
}

void AnimatorComponent::SamplePose(float deltaTime)
{
    if (!m_cachedSkeleton || !m_cachedSkeleton->GetSkeleton()) return;

    auto& skeleton = *m_cachedSkeleton->GetSkeleton();
    auto& pose = m_cachedSkeleton->GetCurrentPose();
    size_t boneCount = skeleton.GetBoneCount();

    // Reset pose to bind pose first
    m_cachedSkeleton->ResetToBindPose();

    // Process crossfade
    if (m_crossFade.active && !m_layers.empty())
    {
        m_crossFade.elapsed += deltaTime;
        float t = m_crossFade.fadeTime > 0.0f
            ? std::clamp(m_crossFade.elapsed / m_crossFade.fadeTime, 0.0f, 1.0f)
            : 1.0f;

        if (t >= 1.0f)
        {
            m_crossFade.active = false;
            // Final transition complete
        }
    }

    // Sample each layer
    for (auto& layer : m_layers)
    {
        if (!layer.state.playing || !layer.state.clip) continue;

        // Advance time
        layer.state.Advance(deltaTime);

        auto& clip = *layer.state.clip;
        float wrappedTime = clip.GetWrappedTime(layer.state.time);

        // Sample each channel
        for (const auto& channel : clip.GetChannels())
        {
            int boneIndex = channel.boneIndex;
            if (boneIndex < 0 || boneIndex >= static_cast<int>(boneCount)) continue;

            // Check bone mask
            if (!layer.boneMask.empty())
            {
                auto it = std::find(layer.boneMask.begin(), layer.boneMask.end(), boneIndex);
                if (it == layer.boneMask.end()) continue;
            }

            auto& localTransform = pose.GetLocalTransform(boneIndex);

            Assets::Vec3 sampledPos = channel.SamplePosition(wrappedTime);
            Assets::Quaternion sampledRot = channel.SampleRotation(wrappedTime);
            Assets::Vec3 sampledScale = channel.SampleScale(wrappedTime);

            if (layer.additive)
            {
                // Additive blending
                localTransform.position = localTransform.position + sampledPos * layer.blendWeight;
                // Quaternion additive is more complex, simplified here
                localTransform.scale.x *= 1.0f + (sampledScale.x - 1.0f) * layer.blendWeight;
                localTransform.scale.y *= 1.0f + (sampledScale.y - 1.0f) * layer.blendWeight;
                localTransform.scale.z *= 1.0f + (sampledScale.z - 1.0f) * layer.blendWeight;
            }
            else
            {
                // Override blending
                localTransform.position = Assets::Vec3::Lerp(localTransform.position, sampledPos, layer.blendWeight);
                localTransform.rotation = Assets::Quaternion::Slerp(localTransform.rotation, sampledRot, layer.blendWeight);
                localTransform.scale = Assets::Vec3::Lerp(localTransform.scale, sampledScale, layer.blendWeight);
            }
        }
    }

    // Compute final global transforms and skinning matrices
    m_cachedSkeleton->UpdateGlobalTransforms();
}

void AnimatorComponent::ApplyLayerBlending()
{
    // Already handled in SamplePose for now
}

void AnimatorComponent::ProcessRootMotion(const Assets::AnimationPose& previousPose)
{
    if (!m_cachedSkeleton || !m_cachedSkeleton->GetSkeleton()) return;

    // Assume bone 0 is root
    const auto& skeleton = *m_cachedSkeleton->GetSkeleton();
    if (skeleton.GetBoneCount() == 0) return;

    // Get current and previous root bone transforms
    const auto& currentPose = m_cachedSkeleton->GetCurrentPose();
    const auto& prevLocal = previousPose.GetLocalTransform(0);
    const auto& currLocal = currentPose.GetLocalTransform(0);

    // Calculate delta
    m_rootMotionDelta = currLocal.position - prevLocal.position;

    // Rotation delta (simplified)
    // In practice, you'd compute the relative rotation
    m_rootRotationDelta = Assets::Quaternion::Identity();
}

} // namespace Aetherion::Scene
