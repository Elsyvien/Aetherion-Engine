#include "Aetherion/Scene/SceneSerializer.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <vector>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace
{
using namespace Aetherion;

std::string ExtractStringValue(const std::string& source, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1)
    {
        return match[1];
    }
    return {};
}

std::optional<std::uint64_t> ExtractUint64(const std::string& source, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1)
    {
        return static_cast<std::uint64_t>(std::stoull(match[1]));
    }
    return std::nullopt;
}

std::optional<float> ExtractFloat(const std::string& source, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1)
    {
        return std::stof(match[1]);
    }
    return std::nullopt;
}

std::vector<float> ExtractFloatArray(const std::string& source, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1)
    {
        std::vector<float> values;
        std::stringstream stream(match[1]);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            if (token.empty())
            {
                continue;
            }
            values.push_back(std::stof(token));
        }
        return values;
    }
    return {};
}

bool ExtractBool(const std::string& source, const std::string& key, bool defaultValue)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(source, match, pattern) && match.size() > 1)
    {
        return match[1] == "true";
    }
    return defaultValue;
}

std::vector<std::string> ExtractEntityBlocks(const std::string& content)
{
    std::vector<std::string> blocks;
    const auto entitiesPos = content.find("\"entities\"");
    if (entitiesPos == std::string::npos)
    {
        return blocks;
    }

    const auto arrayStart = content.find('[', entitiesPos);
    if (arrayStart == std::string::npos)
    {
        return blocks;
    }

    std::size_t cursor = arrayStart + 1;
    while (cursor < content.size())
    {
        while (cursor < content.size() && std::isspace(static_cast<unsigned char>(content[cursor])))
        {
            ++cursor;
        }

        if (cursor >= content.size() || content[cursor] != '{')
        {
            break;
        }

        std::size_t depth = 0;
        const std::size_t blockStart = cursor;
        while (cursor < content.size())
        {
            if (content[cursor] == '{')
            {
                ++depth;
            }
            else if (content[cursor] == '}')
            {
                if (--depth == 0)
                {
                    ++cursor; // include closing brace
                    blocks.emplace_back(content.substr(blockStart, cursor - blockStart));
                    break;
                }
            }
            ++cursor;
        }
    }

    return blocks;
}
} // namespace

namespace Aetherion::Scene
{
SceneSerializer::SceneSerializer(Runtime::EngineContext& context)
    : m_context(context)
{
}

bool SceneSerializer::Save(const Scene& scene, const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path.parent_path()))
    {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out(path);
    if (!out.is_open())
    {
        return false;
    }

    out << "{\n";
    out << "  \"name\": \"" << scene.GetName() << "\",\n";
    out << "  \"entities\": [\n";

    const auto& entities = scene.GetEntities();
    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        const auto& entity = entities[i];
        if (!entity)
        {
            continue;
        }

        out << "    {\n";
        out << "      \"id\": " << entity->GetId() << ",\n";
        out << "      \"name\": \"" << entity->GetName() << "\",\n";
        out << "      \"components\": {\n";

        if (auto transform = entity->GetComponent<TransformComponent>())
        {
            out << "        \"Transform\": {\n";
            out << "          \"position\": [" << transform->GetPositionX() << ", " << transform->GetPositionY() << "],\n";
            out << "          \"rotationZ\": " << transform->GetRotationZDegrees() << ",\n";
            out << "          \"scale\": [" << transform->GetScaleX() << ", " << transform->GetScaleY() << "]\n";
            out << "        }";
        }

        if (auto mesh = entity->GetComponent<MeshRendererComponent>())
        {
            if (entity->GetComponent<TransformComponent>())
            {
                out << ",\n";
            }
            out << "        \"MeshRenderer\": {\n";
            const auto color = mesh->GetColor();
            out << "          \"visible\": " << (mesh->IsVisible() ? "true" : "false") << ",\n";
            out << "          \"color\": [" << color[0] << ", " << color[1] << ", " << color[2] << "],\n";
            out << "          \"rotationSpeed\": " << mesh->GetRotationSpeedDegPerSec() << "\n";
            out << "        }";
        }

        out << "\n";
        out << "      }\n";
        out << "    }" << (i + 1 < entities.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";

    return true;
}

std::shared_ptr<Scene> SceneSerializer::Load(const std::filesystem::path& path) const
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    auto scene = std::make_shared<Scene>(ExtractStringValue(content, "name"));
    scene->BindContext(m_context);

    const auto entityBlocks = ExtractEntityBlocks(content);
    for (const auto& block : entityBlocks)
    {
        const auto id = ExtractUint64(block, "id").value_or(0);
        auto entity = std::make_shared<Entity>(id, ExtractStringValue(block, "name"));

        const auto position = ExtractFloatArray(block, "position");
        const auto scale = ExtractFloatArray(block, "scale");
        const auto rotation = ExtractFloat(block, "rotationZ");
        if (!position.empty() || !scale.empty() || rotation.has_value())
        {
            auto transform = std::make_shared<TransformComponent>();
            if (position.size() >= 2)
            {
                transform->SetPosition(position[0], position[1]);
            }
            if (rotation.has_value())
            {
                transform->SetRotationZDegrees(*rotation);
            }
            if (scale.size() >= 2)
            {
                transform->SetScale(scale[0], scale[1]);
            }
            entity->AddComponent(transform);
        }

        const auto visible = ExtractBool(block, "visible", true);
        const auto meshColor = ExtractFloatArray(block, "color");
        const auto rotationSpeed = ExtractFloat(block, "rotationSpeed");
        if (!meshColor.empty() || rotationSpeed.has_value() || block.find("MeshRenderer") != std::string::npos)
        {
            auto mesh = std::make_shared<MeshRendererComponent>();
            mesh->SetVisible(visible);
            if (meshColor.size() >= 3)
            {
                mesh->SetColor(meshColor[0], meshColor[1], meshColor[2]);
            }
            if (rotationSpeed.has_value())
            {
                mesh->SetRotationSpeedDegPerSec(*rotationSpeed);
            }
            entity->AddComponent(mesh);
        }

        scene->AddEntity(entity);
    }

    return scene;
}

std::shared_ptr<Scene> SceneSerializer::CreateDefaultScene() const
{
    auto scene = std::make_shared<Scene>("Main Scene");
    scene->BindContext(m_context);

    auto viewportEntity = std::make_shared<Entity>(1, "Viewport Quad");
    auto transform = std::make_shared<TransformComponent>();
    auto mesh = std::make_shared<MeshRendererComponent>();
    mesh->SetRotationSpeedDegPerSec(15.0f);
    viewportEntity->AddComponent(transform);
    viewportEntity->AddComponent(mesh);
    scene->AddEntity(viewportEntity);

    if (auto assets = m_context.GetAssetRegistry())
    {
        assets->Scan(std::filesystem::current_path().string());
    }

    return scene;
}
} // namespace Aetherion::Scene
