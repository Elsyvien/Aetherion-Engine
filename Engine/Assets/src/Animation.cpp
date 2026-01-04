#include "Aetherion/Assets/Animation.h"
#include <algorithm>
#include <cmath>

namespace Aetherion::Assets
{

// ============================================================================
// Mat4 Implementation
// ============================================================================

Mat4 Mat4::FromTRS(const Vec3& translation, const Quaternion& rotation, const Vec3& scale)
{
    Mat4 result;

    // Convert quaternion to rotation matrix
    float xx = rotation.x * rotation.x;
    float yy = rotation.y * rotation.y;
    float zz = rotation.z * rotation.z;
    float xy = rotation.x * rotation.y;
    float xz = rotation.x * rotation.z;
    float yz = rotation.y * rotation.z;
    float wx = rotation.w * rotation.x;
    float wy = rotation.w * rotation.y;
    float wz = rotation.w * rotation.z;

    // Column-major order
    result.m[0] = (1.0f - 2.0f * (yy + zz)) * scale.x;
    result.m[1] = (2.0f * (xy + wz)) * scale.x;
    result.m[2] = (2.0f * (xz - wy)) * scale.x;
    result.m[3] = 0.0f;

    result.m[4] = (2.0f * (xy - wz)) * scale.y;
    result.m[5] = (1.0f - 2.0f * (xx + zz)) * scale.y;
    result.m[6] = (2.0f * (yz + wx)) * scale.y;
    result.m[7] = 0.0f;

    result.m[8] = (2.0f * (xz + wy)) * scale.z;
    result.m[9] = (2.0f * (yz - wx)) * scale.z;
    result.m[10] = (1.0f - 2.0f * (xx + yy)) * scale.z;
    result.m[11] = 0.0f;

    result.m[12] = translation.x;
    result.m[13] = translation.y;
    result.m[14] = translation.z;
    result.m[15] = 1.0f;

    return result;
}

Mat4 Mat4::operator*(const Mat4& other) const
{
    Mat4 result;

    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.m[col * 4 + row] =
                m[0 * 4 + row] * other.m[col * 4 + 0] +
                m[1 * 4 + row] * other.m[col * 4 + 1] +
                m[2 * 4 + row] * other.m[col * 4 + 2] +
                m[3 * 4 + row] * other.m[col * 4 + 3];
        }
    }

    return result;
}

// ============================================================================
// AnimationChannel Implementation
// ============================================================================

namespace
{
    template<typename T, typename KeyType>
    T SampleKeyframes(const std::vector<KeyType>& keys, float t, const T& defaultValue)
    {
        if (keys.empty()) return defaultValue;
        if (keys.size() == 1) return keys[0].value;

        // Find surrounding keyframes
        size_t nextIndex = 0;
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (keys[i].time > t)
            {
                nextIndex = i;
                break;
            }
            nextIndex = i;
        }

        if (nextIndex == 0)
        {
            return keys[0].value;
        }

        size_t prevIndex = nextIndex - 1;
        if (nextIndex >= keys.size())
        {
            return keys.back().value;
        }

        const auto& prev = keys[prevIndex];
        const auto& next = keys[nextIndex];

        float duration = next.time - prev.time;
        if (duration < 0.0001f)
        {
            return prev.value;
        }

        float factor = (t - prev.time) / duration;
        factor = std::clamp(factor, 0.0f, 1.0f);

        if (prev.interpolation == InterpolationType::Step)
        {
            return prev.value;
        }

        // Linear interpolation (specialized for Quaternion vs Vec3)
        return defaultValue; // Placeholder, specialized below
    }
}

Vec3 AnimationChannel::SamplePosition(float t) const
{
    if (positionKeys.empty()) return Vec3{0.0f, 0.0f, 0.0f};
    if (positionKeys.size() == 1) return positionKeys[0].value;

    size_t nextIndex = 0;
    for (size_t i = 0; i < positionKeys.size(); ++i)
    {
        if (positionKeys[i].time > t)
        {
            nextIndex = i;
            break;
        }
        nextIndex = i + 1;
    }

    if (nextIndex == 0) return positionKeys[0].value;
    if (nextIndex >= positionKeys.size()) return positionKeys.back().value;

    const auto& prev = positionKeys[nextIndex - 1];
    const auto& next = positionKeys[nextIndex];

    float duration = next.time - prev.time;
    if (duration < 0.0001f) return prev.value;

    float factor = std::clamp((t - prev.time) / duration, 0.0f, 1.0f);

    if (prev.interpolation == InterpolationType::Step)
    {
        return prev.value;
    }

    return Vec3::Lerp(prev.value, next.value, factor);
}

Quaternion AnimationChannel::SampleRotation(float t) const
{
    if (rotationKeys.empty()) return Quaternion::Identity();
    if (rotationKeys.size() == 1) return rotationKeys[0].value;

    size_t nextIndex = 0;
    for (size_t i = 0; i < rotationKeys.size(); ++i)
    {
        if (rotationKeys[i].time > t)
        {
            nextIndex = i;
            break;
        }
        nextIndex = i + 1;
    }

    if (nextIndex == 0) return rotationKeys[0].value;
    if (nextIndex >= rotationKeys.size()) return rotationKeys.back().value;

    const auto& prev = rotationKeys[nextIndex - 1];
    const auto& next = rotationKeys[nextIndex];

    float duration = next.time - prev.time;
    if (duration < 0.0001f) return prev.value;

    float factor = std::clamp((t - prev.time) / duration, 0.0f, 1.0f);

    if (prev.interpolation == InterpolationType::Step)
    {
        return prev.value;
    }

    return Quaternion::Slerp(prev.value, next.value, factor);
}

Vec3 AnimationChannel::SampleScale(float t) const
{
    if (scaleKeys.empty()) return Vec3{1.0f, 1.0f, 1.0f};
    if (scaleKeys.size() == 1) return scaleKeys[0].value;

    size_t nextIndex = 0;
    for (size_t i = 0; i < scaleKeys.size(); ++i)
    {
        if (scaleKeys[i].time > t)
        {
            nextIndex = i;
            break;
        }
        nextIndex = i + 1;
    }

    if (nextIndex == 0) return scaleKeys[0].value;
    if (nextIndex >= scaleKeys.size()) return scaleKeys.back().value;

    const auto& prev = scaleKeys[nextIndex - 1];
    const auto& next = scaleKeys[nextIndex];

    float duration = next.time - prev.time;
    if (duration < 0.0001f) return prev.value;

    float factor = std::clamp((t - prev.time) / duration, 0.0f, 1.0f);

    if (prev.interpolation == InterpolationType::Step)
    {
        return prev.value;
    }

    return Vec3::Lerp(prev.value, next.value, factor);
}

// ============================================================================
// AnimationClip Implementation
// ============================================================================

void AnimationClip::BindToSkeleton(const Skeleton& skeleton)
{
    for (auto& channel : m_channels)
    {
        channel.boneIndex = skeleton.FindBoneIndex(channel.boneName);
    }
}

float AnimationClip::GetWrappedTime(float time) const
{
    if (m_duration <= 0.0f) return 0.0f;

    switch (m_wrapMode)
    {
    case AnimationWrapMode::Once:
    case AnimationWrapMode::ClampForever:
        return std::clamp(time, 0.0f, m_duration);

    case AnimationWrapMode::Loop:
        {
            float wrapped = std::fmod(time, m_duration);
            return wrapped < 0.0f ? wrapped + m_duration : wrapped;
        }

    case AnimationWrapMode::PingPong:
        {
            float cycle = m_duration * 2.0f;
            float wrapped = std::fmod(time, cycle);
            if (wrapped < 0.0f) wrapped += cycle;
            if (wrapped > m_duration)
            {
                wrapped = cycle - wrapped;
            }
            return wrapped;
        }
    }

    return time;
}

// ============================================================================
// AnimationPose Implementation
// ============================================================================

void AnimationPose::ComputeGlobalTransforms(const Skeleton& skeleton)
{
    const auto& bones = skeleton.GetBones();
    size_t boneCount = std::min(m_localTransforms.size(), bones.size());

    for (size_t i = 0; i < boneCount; ++i)
    {
        const auto& local = m_localTransforms[i];
        Mat4 localMatrix = Mat4::FromTRS(local.position, local.rotation, local.scale);

        int parentIndex = bones[i].parentIndex;
        if (parentIndex >= 0 && parentIndex < static_cast<int>(boneCount))
        {
            m_globalTransforms[i] = m_globalTransforms[parentIndex] * localMatrix;
        }
        else
        {
            m_globalTransforms[i] = localMatrix;
        }

        // Skinning matrix = global * inverse bind pose
        m_skinningMatrices[i] = m_globalTransforms[i] * bones[i].inverseBindPose;
    }
}

AnimationPose AnimationPose::Blend(const AnimationPose& a, const AnimationPose& b, float t)
{
    size_t count = std::min(a.m_localTransforms.size(), b.m_localTransforms.size());
    AnimationPose result(count);

    for (size_t i = 0; i < count; ++i)
    {
        result.m_localTransforms[i] = BoneTransform::Blend(
            a.m_localTransforms[i], b.m_localTransforms[i], t);
    }

    return result;
}

} // namespace Aetherion::Assets
