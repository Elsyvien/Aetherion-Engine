#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <array>
#include <cmath>

namespace Aetherion::Assets
{

// ============================================================================
// Math Types for Animation
// ============================================================================

struct Vec3
{
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }

    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
    {
        return a + (b - a) * t;
    }
};

struct Quaternion
{
    float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};

    Quaternion() = default;
    Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quaternion Identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    float Dot(const Quaternion& other) const
    {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    Quaternion operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
    Quaternion operator+(const Quaternion& other) const { return {x + other.x, y + other.y, z + other.z, w + other.w}; }
    Quaternion operator-() const { return {-x, -y, -z, -w}; }

    Quaternion Normalized() const
    {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        if (len < 0.0001f) return Identity();
        return {x / len, y / len, z / len, w / len};
    }

    // Spherical linear interpolation
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        float dot = a.Dot(b);
        Quaternion b2 = b;

        // If dot < 0, negate one quaternion to take shorter path
        if (dot < 0.0f)
        {
            b2 = -b;
            dot = -dot;
        }

        // If very close, use linear interpolation
        if (dot > 0.9995f)
        {
            return (a * (1.0f - t) + b2 * t).Normalized();
        }

        float theta0 = std::acos(dot);
        float theta = theta0 * t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);

        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;

        return (a * s0 + b2 * s1).Normalized();
    }
};

struct Mat4
{
    std::array<float, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    static Mat4 Identity()
    {
        Mat4 result;
        return result;
    }

    static Mat4 FromTRS(const Vec3& translation, const Quaternion& rotation, const Vec3& scale);

    Mat4 operator*(const Mat4& other) const;
};

// ============================================================================
// Bone / Skeleton Structures
// ============================================================================

constexpr int INVALID_BONE_INDEX = -1;

struct Bone
{
    std::string name;
    int parentIndex{INVALID_BONE_INDEX};

    // Bind pose (inverse bind matrix for skinning)
    Mat4 inverseBindPose{Mat4::Identity()};

    // Local rest pose transform
    Vec3 restPosition{0.0f, 0.0f, 0.0f};
    Quaternion restRotation{Quaternion::Identity()};
    Vec3 restScale{1.0f, 1.0f, 1.0f};
};

class Skeleton
{
public:
    Skeleton() = default;
    explicit Skeleton(const std::string& name) : m_name(name) {}

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    [[nodiscard]] size_t GetBoneCount() const noexcept { return m_bones.size(); }
    [[nodiscard]] const std::vector<Bone>& GetBones() const noexcept { return m_bones; }

    int AddBone(const Bone& bone)
    {
        int index = static_cast<int>(m_bones.size());
        m_bones.push_back(bone);
        m_boneNameToIndex[bone.name] = index;
        return index;
    }

    [[nodiscard]] int FindBoneIndex(const std::string& name) const
    {
        auto it = m_boneNameToIndex.find(name);
        return it != m_boneNameToIndex.end() ? it->second : INVALID_BONE_INDEX;
    }

    [[nodiscard]] const Bone* GetBone(int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_bones.size())) return nullptr;
        return &m_bones[index];
    }

private:
    std::string m_name;
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, int> m_boneNameToIndex;
};

// ============================================================================
// Animation Keyframes
// ============================================================================

enum class InterpolationType : uint8_t
{
    Step,       // No interpolation, snap to value
    Linear,     // Linear interpolation
    Cubic       // Cubic spline interpolation
};

template<typename T>
struct Keyframe
{
    float time{0.0f};
    T value{};
    InterpolationType interpolation{InterpolationType::Linear};

    // Tangents for cubic interpolation
    T inTangent{};
    T outTangent{};
};

using PositionKeyframe = Keyframe<Vec3>;
using RotationKeyframe = Keyframe<Quaternion>;
using ScaleKeyframe = Keyframe<Vec3>;

// ============================================================================
// Animation Channel (tracks one bone's transforms)
// ============================================================================

struct AnimationChannel
{
    std::string boneName;
    int boneIndex{INVALID_BONE_INDEX}; // Cached after binding to skeleton

    std::vector<PositionKeyframe> positionKeys;
    std::vector<RotationKeyframe> rotationKeys;
    std::vector<ScaleKeyframe> scaleKeys;

    // Sample position at time t
    [[nodiscard]] Vec3 SamplePosition(float t) const;
    [[nodiscard]] Quaternion SampleRotation(float t) const;
    [[nodiscard]] Vec3 SampleScale(float t) const;
};

// ============================================================================
// Animation Clip
// ============================================================================

enum class AnimationWrapMode : uint8_t
{
    Once,           // Play once and stop
    Loop,           // Loop continuously
    PingPong,       // Play forward, then backward
    ClampForever    // Play once, hold last frame
};

class AnimationClip
{
public:
    AnimationClip() = default;
    explicit AnimationClip(const std::string& name) : m_name(name) {}

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] float GetDuration() const noexcept { return m_duration; }
    void SetDuration(float duration) { m_duration = duration; }

    [[nodiscard]] float GetTicksPerSecond() const noexcept { return m_ticksPerSecond; }
    void SetTicksPerSecond(float tps) { m_ticksPerSecond = tps; }

    [[nodiscard]] AnimationWrapMode GetWrapMode() const noexcept { return m_wrapMode; }
    void SetWrapMode(AnimationWrapMode mode) { m_wrapMode = mode; }

    [[nodiscard]] const std::vector<AnimationChannel>& GetChannels() const noexcept { return m_channels; }
    std::vector<AnimationChannel>& GetChannels() noexcept { return m_channels; }

    void AddChannel(const AnimationChannel& channel) { m_channels.push_back(channel); }

    // Bind channel bone indices to skeleton
    void BindToSkeleton(const Skeleton& skeleton);

    // Get wrapped time based on wrap mode
    [[nodiscard]] float GetWrappedTime(float time) const;

private:
    std::string m_name;
    float m_duration{0.0f};
    float m_ticksPerSecond{30.0f};
    AnimationWrapMode m_wrapMode{AnimationWrapMode::Loop};
    std::vector<AnimationChannel> m_channels;
};

// ============================================================================
// Animation Pose (result of sampling an animation)
// ============================================================================

struct BoneTransform
{
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{Quaternion::Identity()};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    static BoneTransform Blend(const BoneTransform& a, const BoneTransform& b, float t)
    {
        BoneTransform result;
        result.position = Vec3::Lerp(a.position, b.position, t);
        result.rotation = Quaternion::Slerp(a.rotation, b.rotation, t);
        result.scale = Vec3::Lerp(a.scale, b.scale, t);
        return result;
    }
};

class AnimationPose
{
public:
    AnimationPose() = default;
    explicit AnimationPose(size_t boneCount)
        : m_localTransforms(boneCount)
        , m_globalTransforms(boneCount)
        , m_skinningMatrices(boneCount)
    {
    }

    void Resize(size_t boneCount)
    {
        m_localTransforms.resize(boneCount);
        m_globalTransforms.resize(boneCount);
        m_skinningMatrices.resize(boneCount);
    }

    [[nodiscard]] size_t GetBoneCount() const noexcept { return m_localTransforms.size(); }

    [[nodiscard]] BoneTransform& GetLocalTransform(size_t index) { return m_localTransforms[index]; }
    [[nodiscard]] const BoneTransform& GetLocalTransform(size_t index) const { return m_localTransforms[index]; }

    [[nodiscard]] Mat4& GetGlobalTransform(size_t index) { return m_globalTransforms[index]; }
    [[nodiscard]] const Mat4& GetGlobalTransform(size_t index) const { return m_globalTransforms[index]; }

    [[nodiscard]] Mat4& GetSkinningMatrix(size_t index) { return m_skinningMatrices[index]; }
    [[nodiscard]] const Mat4& GetSkinningMatrix(size_t index) const { return m_skinningMatrices[index]; }

    [[nodiscard]] const std::vector<Mat4>& GetSkinningMatrices() const noexcept { return m_skinningMatrices; }

    // Compute global transforms and skinning matrices from local transforms
    void ComputeGlobalTransforms(const Skeleton& skeleton);

    // Blend two poses
    static AnimationPose Blend(const AnimationPose& a, const AnimationPose& b, float t);

private:
    std::vector<BoneTransform> m_localTransforms;
    std::vector<Mat4> m_globalTransforms;
    std::vector<Mat4> m_skinningMatrices;
};

// ============================================================================
// Animation State (runtime playback state)
// ============================================================================

struct AnimationState
{
    std::shared_ptr<AnimationClip> clip;
    float time{0.0f};
    float speed{1.0f};
    float weight{1.0f};
    bool playing{false};
    bool paused{false};

    void Reset()
    {
        time = 0.0f;
        playing = false;
        paused = false;
    }

    void Play()
    {
        playing = true;
        paused = false;
    }

    void Pause() { paused = true; }
    void Resume() { paused = false; }
    void Stop() { Reset(); }

    // Advance time and return true if animation ended (for Once mode)
    bool Advance(float deltaTime)
    {
        if (!playing || paused || !clip) return false;

        time += deltaTime * speed;

        if (clip->GetWrapMode() == AnimationWrapMode::Once && time >= clip->GetDuration())
        {
            time = clip->GetDuration();
            playing = false;
            return true;
        }

        return false;
    }
};

// ============================================================================
// Animation Blend Tree Node Types
// ============================================================================

enum class BlendNodeType : uint8_t
{
    Clip,       // Single animation clip
    Blend1D,    // 1D blend between clips based on parameter
    Blend2D,    // 2D blend (e.g., for locomotion)
    Additive,   // Additive blend
    Override    // Override with mask
};

} // namespace Aetherion::Assets
