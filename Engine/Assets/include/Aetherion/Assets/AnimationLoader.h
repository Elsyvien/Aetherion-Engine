#pragma once

#include "Aetherion/Assets/Animation.h"
#include <filesystem>
#include <memory>
#include <string>

namespace Aetherion::Assets
{

/// @brief Load and save animation assets in JSON format
class AnimationLoader
{
public:
    /// @brief Load a skeleton from a JSON file
    /// @param path Path to the .skeleton.json file
    /// @return Loaded skeleton or nullptr on failure
    static std::shared_ptr<Skeleton> LoadSkeleton(const std::filesystem::path& path);

    /// @brief Save a skeleton to a JSON file
    /// @param skeleton The skeleton to save
    /// @param path Path to save to (should end in .skeleton.json)
    /// @return True on success
    static bool SaveSkeleton(const Skeleton& skeleton, const std::filesystem::path& path);

    /// @brief Load an animation clip from a JSON file
    /// @param path Path to the .anim.json file
    /// @return Loaded animation clip or nullptr on failure
    static std::shared_ptr<AnimationClip> LoadAnimation(const std::filesystem::path& path);

    /// @brief Save an animation clip to a JSON file
    /// @param clip The animation clip to save
    /// @param path Path to save to (should end in .anim.json)
    /// @return True on success
    static bool SaveAnimation(const AnimationClip& clip, const std::filesystem::path& path);

    /// @brief Create a simple test animation for debugging
    /// @param duration Animation duration in seconds
    /// @param boneName Name of the bone to animate
    /// @return A test animation clip
    static std::shared_ptr<AnimationClip> CreateTestAnimation(
        float duration = 2.0f,
        const std::string& boneName = "Root");

    /// @brief Create a simple test skeleton for debugging
    /// @return A test skeleton with a few bones
    static std::shared_ptr<Skeleton> CreateTestSkeleton();
};

} // namespace Aetherion::Assets
