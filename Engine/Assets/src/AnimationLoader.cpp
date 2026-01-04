#include "Aetherion/Assets/AnimationLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>

namespace Aetherion::Assets
{

using json = nlohmann::json;

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

namespace
{

json Vec3ToJson(const Vec3& v)
{
    return json::array({v.x, v.y, v.z});
}

Vec3 JsonToVec3(const json& j)
{
    if (!j.is_array() || j.size() < 3) return Vec3{0.0f, 0.0f, 0.0f};
    return Vec3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

json QuaternionToJson(const Quaternion& q)
{
    return json::array({q.x, q.y, q.z, q.w});
}

Quaternion JsonToQuaternion(const json& j)
{
    if (!j.is_array() || j.size() < 4) return Quaternion::Identity();
    return Quaternion{j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

json Mat4ToJson(const Mat4& m)
{
    json arr = json::array();
    for (int i = 0; i < 16; ++i)
    {
        arr.push_back(m.m[i]);
    }
    return arr;
}

Mat4 JsonToMat4(const json& j)
{
    Mat4 result;
    if (j.is_array() && j.size() >= 16)
    {
        for (int i = 0; i < 16; ++i)
        {
            result.m[i] = j[i].get<float>();
        }
    }
    return result;
}

std::string InterpolationTypeToString(InterpolationType type)
{
    switch (type)
    {
    case InterpolationType::Step: return "step";
    case InterpolationType::Linear: return "linear";
    case InterpolationType::Cubic: return "cubic";
    }
    return "linear";
}

InterpolationType StringToInterpolationType(const std::string& s)
{
    if (s == "step") return InterpolationType::Step;
    if (s == "cubic") return InterpolationType::Cubic;
    return InterpolationType::Linear;
}

std::string WrapModeToString(AnimationWrapMode mode)
{
    switch (mode)
    {
    case AnimationWrapMode::Once: return "once";
    case AnimationWrapMode::Loop: return "loop";
    case AnimationWrapMode::PingPong: return "pingpong";
    case AnimationWrapMode::ClampForever: return "clampforever";
    }
    return "loop";
}

AnimationWrapMode StringToWrapMode(const std::string& s)
{
    if (s == "once") return AnimationWrapMode::Once;
    if (s == "pingpong") return AnimationWrapMode::PingPong;
    if (s == "clampforever") return AnimationWrapMode::ClampForever;
    return AnimationWrapMode::Loop;
}

} // anonymous namespace

// ============================================================================
// Skeleton Loading/Saving
// ============================================================================

std::shared_ptr<Skeleton> AnimationLoader::LoadSkeleton(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return nullptr;
    }

    try
    {
        json j;
        file >> j;

        auto skeleton = std::make_shared<Skeleton>(j.value("name", "Skeleton"));

        if (j.contains("bones") && j["bones"].is_array())
        {
            for (const auto& boneJson : j["bones"])
            {
                Bone bone;
                bone.name = boneJson.value("name", "");
                bone.parentIndex = boneJson.value("parentIndex", INVALID_BONE_INDEX);

                if (boneJson.contains("inverseBindPose"))
                {
                    bone.inverseBindPose = JsonToMat4(boneJson["inverseBindPose"]);
                }
                if (boneJson.contains("restPosition"))
                {
                    bone.restPosition = JsonToVec3(boneJson["restPosition"]);
                }
                if (boneJson.contains("restRotation"))
                {
                    bone.restRotation = JsonToQuaternion(boneJson["restRotation"]);
                }
                if (boneJson.contains("restScale"))
                {
                    bone.restScale = JsonToVec3(boneJson["restScale"]);
                }

                skeleton->AddBone(bone);
            }
        }

        return skeleton;
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

bool AnimationLoader::SaveSkeleton(const Skeleton& skeleton, const std::filesystem::path& path)
{
    try
    {
        json j;
        j["name"] = skeleton.GetName();
        j["version"] = 1;

        json bonesJson = json::array();
        for (const auto& bone : skeleton.GetBones())
        {
            json boneJson;
            boneJson["name"] = bone.name;
            boneJson["parentIndex"] = bone.parentIndex;
            boneJson["inverseBindPose"] = Mat4ToJson(bone.inverseBindPose);
            boneJson["restPosition"] = Vec3ToJson(bone.restPosition);
            boneJson["restRotation"] = QuaternionToJson(bone.restRotation);
            boneJson["restScale"] = Vec3ToJson(bone.restScale);
            bonesJson.push_back(boneJson);
        }
        j["bones"] = bonesJson;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

// ============================================================================
// Animation Loading/Saving
// ============================================================================

std::shared_ptr<AnimationClip> AnimationLoader::LoadAnimation(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return nullptr;
    }

    try
    {
        json j;
        file >> j;

        auto clip = std::make_shared<AnimationClip>(j.value("name", "Animation"));
        clip->SetDuration(j.value("duration", 1.0f));
        clip->SetTicksPerSecond(j.value("ticksPerSecond", 30.0f));
        clip->SetWrapMode(StringToWrapMode(j.value("wrapMode", "loop")));

        if (j.contains("channels") && j["channels"].is_array())
        {
            for (const auto& channelJson : j["channels"])
            {
                AnimationChannel channel;
                channel.boneName = channelJson.value("boneName", "");

                // Position keyframes
                if (channelJson.contains("positionKeys") && channelJson["positionKeys"].is_array())
                {
                    for (const auto& keyJson : channelJson["positionKeys"])
                    {
                        PositionKeyframe key;
                        key.time = keyJson.value("time", 0.0f);
                        key.value = JsonToVec3(keyJson["value"]);
                        key.interpolation = StringToInterpolationType(keyJson.value("interpolation", "linear"));
                        channel.positionKeys.push_back(key);
                    }
                }

                // Rotation keyframes
                if (channelJson.contains("rotationKeys") && channelJson["rotationKeys"].is_array())
                {
                    for (const auto& keyJson : channelJson["rotationKeys"])
                    {
                        RotationKeyframe key;
                        key.time = keyJson.value("time", 0.0f);
                        key.value = JsonToQuaternion(keyJson["value"]);
                        key.interpolation = StringToInterpolationType(keyJson.value("interpolation", "linear"));
                        channel.rotationKeys.push_back(key);
                    }
                }

                // Scale keyframes
                if (channelJson.contains("scaleKeys") && channelJson["scaleKeys"].is_array())
                {
                    for (const auto& keyJson : channelJson["scaleKeys"])
                    {
                        ScaleKeyframe key;
                        key.time = keyJson.value("time", 0.0f);
                        key.value = JsonToVec3(keyJson["value"]);
                        key.interpolation = StringToInterpolationType(keyJson.value("interpolation", "linear"));
                        channel.scaleKeys.push_back(key);
                    }
                }

                clip->AddChannel(channel);
            }
        }

        return clip;
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

bool AnimationLoader::SaveAnimation(const AnimationClip& clip, const std::filesystem::path& path)
{
    try
    {
        json j;
        j["name"] = clip.GetName();
        j["version"] = 1;
        j["duration"] = clip.GetDuration();
        j["ticksPerSecond"] = clip.GetTicksPerSecond();
        j["wrapMode"] = WrapModeToString(clip.GetWrapMode());

        json channelsJson = json::array();
        for (const auto& channel : clip.GetChannels())
        {
            json channelJson;
            channelJson["boneName"] = channel.boneName;

            // Position keyframes
            json posKeys = json::array();
            for (const auto& key : channel.positionKeys)
            {
                json keyJson;
                keyJson["time"] = key.time;
                keyJson["value"] = Vec3ToJson(key.value);
                keyJson["interpolation"] = InterpolationTypeToString(key.interpolation);
                posKeys.push_back(keyJson);
            }
            channelJson["positionKeys"] = posKeys;

            // Rotation keyframes
            json rotKeys = json::array();
            for (const auto& key : channel.rotationKeys)
            {
                json keyJson;
                keyJson["time"] = key.time;
                keyJson["value"] = QuaternionToJson(key.value);
                keyJson["interpolation"] = InterpolationTypeToString(key.interpolation);
                rotKeys.push_back(keyJson);
            }
            channelJson["rotationKeys"] = rotKeys;

            // Scale keyframes
            json scaleKeys = json::array();
            for (const auto& key : channel.scaleKeys)
            {
                json keyJson;
                keyJson["time"] = key.time;
                keyJson["value"] = Vec3ToJson(key.value);
                keyJson["interpolation"] = InterpolationTypeToString(key.interpolation);
                scaleKeys.push_back(keyJson);
            }
            channelJson["scaleKeys"] = scaleKeys;

            channelsJson.push_back(channelJson);
        }
        j["channels"] = channelsJson;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

// ============================================================================
// Test/Debug Helpers
// ============================================================================

std::shared_ptr<AnimationClip> AnimationLoader::CreateTestAnimation(float duration, const std::string& boneName)
{
    auto clip = std::make_shared<AnimationClip>("TestAnimation");
    clip->SetDuration(duration);
    clip->SetWrapMode(AnimationWrapMode::Loop);

    AnimationChannel channel;
    channel.boneName = boneName;

    // Create a simple rotation animation (360 degrees around Y axis)
    constexpr int numKeys = 5;
    for (int i = 0; i < numKeys; ++i)
    {
        float t = static_cast<float>(i) / (numKeys - 1) * duration;
        float angle = static_cast<float>(i) / (numKeys - 1) * 3.14159f * 2.0f;

        // Position: bobbing up and down
        PositionKeyframe posKey;
        posKey.time = t;
        posKey.value = Vec3{0.0f, std::sin(angle) * 0.5f, 0.0f};
        posKey.interpolation = InterpolationType::Linear;
        channel.positionKeys.push_back(posKey);

        // Rotation: spinning around Y
        RotationKeyframe rotKey;
        rotKey.time = t;
        // Convert axis-angle to quaternion (Y-axis rotation)
        float halfAngle = angle * 0.5f;
        rotKey.value = Quaternion{0.0f, std::sin(halfAngle), 0.0f, std::cos(halfAngle)};
        rotKey.interpolation = InterpolationType::Linear;
        channel.rotationKeys.push_back(rotKey);

        // Scale: pulsing
        ScaleKeyframe scaleKey;
        scaleKey.time = t;
        float s = 1.0f + std::sin(angle) * 0.2f;
        scaleKey.value = Vec3{s, s, s};
        scaleKey.interpolation = InterpolationType::Linear;
        channel.scaleKeys.push_back(scaleKey);
    }

    clip->AddChannel(channel);
    return clip;
}

std::shared_ptr<Skeleton> AnimationLoader::CreateTestSkeleton()
{
    auto skeleton = std::make_shared<Skeleton>("TestSkeleton");

    // Root bone
    Bone root;
    root.name = "Root";
    root.parentIndex = INVALID_BONE_INDEX;
    root.restPosition = Vec3{0.0f, 0.0f, 0.0f};
    root.restRotation = Quaternion::Identity();
    root.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(root);

    // Spine
    Bone spine;
    spine.name = "Spine";
    spine.parentIndex = 0;
    spine.restPosition = Vec3{0.0f, 1.0f, 0.0f};
    spine.restRotation = Quaternion::Identity();
    spine.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(spine);

    // Head
    Bone head;
    head.name = "Head";
    head.parentIndex = 1;
    head.restPosition = Vec3{0.0f, 0.5f, 0.0f};
    head.restRotation = Quaternion::Identity();
    head.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(head);

    // Left arm
    Bone leftArm;
    leftArm.name = "LeftArm";
    leftArm.parentIndex = 1;
    leftArm.restPosition = Vec3{-0.5f, 0.3f, 0.0f};
    leftArm.restRotation = Quaternion::Identity();
    leftArm.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(leftArm);

    // Right arm
    Bone rightArm;
    rightArm.name = "RightArm";
    rightArm.parentIndex = 1;
    rightArm.restPosition = Vec3{0.5f, 0.3f, 0.0f};
    rightArm.restRotation = Quaternion::Identity();
    rightArm.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(rightArm);

    // Left leg
    Bone leftLeg;
    leftLeg.name = "LeftLeg";
    leftLeg.parentIndex = 0;
    leftLeg.restPosition = Vec3{-0.2f, -0.5f, 0.0f};
    leftLeg.restRotation = Quaternion::Identity();
    leftLeg.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(leftLeg);

    // Right leg
    Bone rightLeg;
    rightLeg.name = "RightLeg";
    rightLeg.parentIndex = 0;
    rightLeg.restPosition = Vec3{0.2f, -0.5f, 0.0f};
    rightLeg.restRotation = Quaternion::Identity();
    rightLeg.restScale = Vec3{1.0f, 1.0f, 1.0f};
    skeleton->AddBone(rightLeg);

    return skeleton;
}

} // namespace Aetherion::Assets
