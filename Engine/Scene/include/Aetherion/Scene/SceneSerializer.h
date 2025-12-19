#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace Aetherion::Runtime
{
class EngineContext;
}

namespace Aetherion::Scene
{
class Scene;

class SceneSerializer
{
public:
    explicit SceneSerializer(Runtime::EngineContext& context);

    [[nodiscard]] bool Save(const Scene& scene, const std::filesystem::path& path) const;
    [[nodiscard]] std::shared_ptr<Scene> Load(const std::filesystem::path& path) const;
    [[nodiscard]] std::shared_ptr<Scene> CreateDefaultScene() const;

private:
    Runtime::EngineContext& m_context;
};
} // namespace Aetherion::Scene
