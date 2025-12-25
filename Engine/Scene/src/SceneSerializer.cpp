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
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace
{
using namespace Aetherion;

std::string ExtractStringValue(const std::string& source, const std::string& key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = source.find(needle);
    if (pos == std::string::npos) return {};
    pos = source.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = source.find('"', pos);
    if (pos == std::string::npos) return {};
    size_t start = pos + 1;
    size_t end = source.find('"', start);
    if (end == std::string::npos) return {};
    return source.substr(start, end - start);
}

std::optional<std::uint64_t> ExtractUint64(const std::string& source, const std::string& key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = source.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos = source.find(':', pos + needle.size());
    if (pos == std::string::npos) return std::nullopt;
    // find first digit
    size_t start = source.find_first_of("0123456789", pos);
    if (start == std::string::npos) return std::nullopt;
    size_t end = start;
    while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end]))) ++end;
    return static_cast<std::uint64_t>(std::stoull(source.substr(start, end - start)));
}

std::optional<float> ExtractFloat(const std::string& source, const std::string& key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = source.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos = source.find(':', pos + needle.size());
    if (pos == std::string::npos) return std::nullopt;
    // find start of number (including '-')
    size_t start = source.find_first_of("-0123456789", pos);
    if (start == std::string::npos) return std::nullopt;
    size_t end = start;
    while (end < source.size())
    {
        char c = source[end];
        if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-')) break;
        ++end;
    }
    try { return std::stof(source.substr(start, end - start)); }
    catch (...) { return std::nullopt; }
}

std::vector<float> ExtractFloatArray(const std::string& source, const std::string& key)
{
    std::vector<float> values;
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = source.find(needle);
    if (pos == std::string::npos) return values;
    pos = source.find('[', pos + needle.size());
    if (pos == std::string::npos) return values;
    size_t end = source.find(']', pos);
    if (end == std::string::npos) return values;
    std::string body = source.substr(pos + 1, end - pos - 1);
    std::stringstream stream(body);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        // trim
        size_t a = token.find_first_not_of(" \t\n\r");
        if (a == std::string::npos) continue;
        size_t b = token.find_last_not_of(" \t\n\r");
        std::string part = token.substr(a, b - a + 1);
        try { values.push_back(std::stof(part)); } catch (...) { }
    }
    return values;
}

bool ExtractBool(const std::string& source, const std::string& key, bool defaultValue)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = source.find(needle);
    if (pos == std::string::npos) return defaultValue;
    pos = source.find(':', pos + needle.size());
    if (pos == std::string::npos) return defaultValue;
    size_t start = source.find_first_not_of(" \t\n\r", pos + 1);
    if (start == std::string::npos) return defaultValue;
    if (source.compare(start, 4, "true") == 0) return true;
    if (source.compare(start, 5, "false") == 0) return false;
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

        bool wroteComponent = false;
        if (auto transform = entity->GetComponent<TransformComponent>()) // Removed the erroneous 'сля'
        {
            out << "        \"Transform\": {\n";
            out << "          \"position\": [" << transform->GetPositionX() << ", " << transform->GetPositionY() << ", " << transform->GetPositionZ() << "],\n";
            out << "          \"rotation\": [" << transform->GetRotationXDegrees() << ", " << transform->GetRotationYDegrees() << ", " << transform->GetRotationZDegrees() << "],\n";
            out << "          \"scale\": [" << transform->GetScaleX() << ", " << transform->GetScaleY() << ", " << transform->GetScaleZ() << "],\n";
            out << "          \"parent\": " << transform->GetParentId() << "\n";
            out << "        }";
            wroteComponent = true;
        }

        if (auto mesh = entity->GetComponent<MeshRendererComponent>()){
            if (wroteComponent)
            {
                out << " ,\n";
            }
            out << "        \"MeshRenderer\": {\n";
            const auto color = mesh->GetColor();
            out << "          \"visible\": " << (mesh->IsVisible() ? "true" : "false") << ",\n";
            out << "          \"color\": [" << color[0] << ", " << color[1] << ", " << color[2] << "],\n";
            out << "          \"rotationSpeed\": " << mesh->GetRotationSpeedDegPerSec() << ",\n";
            out << "          \"albedoTexture\": \"" << mesh->GetAlbedoTextureId() << "\",\n";
            out << "          \"meshId\": \"" << mesh->GetMeshAssetId() << "\"\n";
            out << "        }";
            wroteComponent = true;
        }

        if (auto light = entity->GetComponent<LightComponent>()){
            if (wroteComponent)
            {
                out << " ,\n";
            }
            out << "        \"Light\": {\n";
            const auto color = light->GetColor();
            const auto ambient = light->GetAmbientColor();
            out << "          \"lightEnabled\": " << (light->IsEnabled() ? "true" : "false") << ",\n";
            out << "          \"lightType\": " << static_cast<int>(light->GetType()) << ",\n";
            out << "          \"lightColor\": [" << color[0] << ", " << color[1] << ", " << color[2] << "],\n";
            out << "          \"lightIntensity\": " << light->GetIntensity() << ",\n";
            out << "          \"lightRange\": " << light->GetRange() << ",\n";
            out << "          \"innerConeAngle\": " << light->GetInnerConeAngle() << ",\n";
            out << "          \"outerConeAngle\": " << light->GetOuterConeAngle() << ",\n";
            out << "          \"lightPrimary\": " << (light->IsPrimary() ? "true" : "false") << ",\n";
            out << "          \"ambientColor\": [" << ambient[0] << ", " << ambient[1] << ", " << ambient[2] << "]\n";
            out << "        }";
            wroteComponent = true;
        }

        if (auto camera = entity->GetComponent<CameraComponent>()){
            if (wroteComponent)
            {
                out << " ,\n";
            }
            out << "        \"Camera\": {\n";
            out << "          \"projectionType\": " << static_cast<int>(camera->GetProjectionType()) << ",\n";
            out << "          \"verticalFov\": " << camera->GetVerticalFov() << ",\n";
            out << "          \"nearClip\": " << camera->GetNearClip() << ",\n";
            out << "          \"farClip\": " << camera->GetFarClip() << ",\n";
            out << "          \"orthographicSize\": " << camera->GetOrthographicSize() << ",\n";
            out << "          \"isPrimary\": " << (camera->IsPrimary() ? "true" : "false") << "\n";
            out << "        }";
            wroteComponent = true;
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
        const auto rotation = ExtractFloatArray(block, "rotation");
        const auto rotationLegacy = ExtractFloat(block, "rotationZ");
        const auto parentId = ExtractUint64(block, "parent").value_or(0);
        if (!position.empty() || !scale.empty() || !rotation.empty() || rotationLegacy.has_value())
        {
            auto transform = std::make_shared<TransformComponent>();
            if (position.size() >= 3)
            {
                transform->SetPosition(position[0], position[1], position[2]);
            }
            else if (position.size() >= 2)
            {
                transform->SetPosition(position[0], position[1], 0.0f);
            }
            if (rotation.size() >= 3)
            {
                transform->SetRotationDegrees(rotation[0], rotation[1], rotation[2]);
            }
            else if (rotationLegacy.has_value())
            {
                transform->SetRotationDegrees(0.0f, 0.0f, *rotationLegacy);
            }
            if (scale.size() >= 3)
            {
                transform->SetScale(scale[0], scale[1], scale[2]);
            }
            else if (scale.size() >= 2)
            {
                transform->SetScale(scale[0], scale[1], 1.0f);
            }
            if (parentId != 0)
            {
                transform->SetParent(parentId);
            }
            entity->AddComponent(transform);
        }

        const auto visible = ExtractBool(block, "visible", true);
        const auto meshColor = ExtractFloatArray(block, "color");
        const auto rotationSpeed = ExtractFloat(block, "rotationSpeed");
        const auto meshId = ExtractStringValue(block, "meshId");
        const auto albedoTexture = ExtractStringValue(block, "albedoTexture");
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
            if (!meshId.empty())
            {
                mesh->SetMeshAssetId(meshId);
            }
            if (!albedoTexture.empty())
            {
                mesh->SetAlbedoTextureId(albedoTexture);
            }
            entity->AddComponent(mesh);
        }

        const auto lightEnabled = ExtractBool(block, "lightEnabled", true);
        const auto lightType = ExtractUint64(block, "lightType");
        const auto lightColor = ExtractFloatArray(block, "lightColor");
        const auto lightIntensity = ExtractFloat(block, "lightIntensity");
        const auto lightRange = ExtractFloat(block, "lightRange");
        const auto lightInnerCone = ExtractFloat(block, "innerConeAngle");
        const auto lightOuterCone = ExtractFloat(block, "outerConeAngle");
        const auto lightPrimary = ExtractBool(block, "lightPrimary", false);
        const auto ambientColor = ExtractFloatArray(block, "ambientColor");
        if (lightType.has_value() || !lightColor.empty() || lightIntensity.has_value() ||
            lightRange.has_value() || lightInnerCone.has_value() || lightOuterCone.has_value() ||
            !ambientColor.empty() || block.find("\"Light\"") != std::string::npos)
        {
            auto light = std::make_shared<LightComponent>();
            light->SetEnabled(lightEnabled);
            if (lightType.has_value())
            {
                light->SetType(static_cast<LightComponent::LightType>(*lightType));
            }
            if (lightColor.size() >= 3)
            {
                light->SetColor(lightColor[0], lightColor[1], lightColor[2]);
            }
            if (lightIntensity.has_value())
            {
                light->SetIntensity(*lightIntensity);
            }
            if (lightRange.has_value())
            {
                light->SetRange(*lightRange);
            }
            if (lightInnerCone.has_value())
            {
                light->SetInnerConeAngle(*lightInnerCone);
            }
            if (lightOuterCone.has_value())
            {
                light->SetOuterConeAngle(*lightOuterCone);
            }
            if (lightPrimary)
            {
                light->SetPrimary(lightPrimary);
            }
            if (ambientColor.size() >= 3)
            {
                light->SetAmbientColor(ambientColor[0], ambientColor[1], ambientColor[2]);
            }
            entity->AddComponent(light);
        }

        const auto cameraProjection = ExtractUint64(block, "projectionType");
        const auto cameraFov = ExtractFloat(block, "verticalFov");
        const auto cameraNear = ExtractFloat(block, "nearClip");
        const auto cameraFar = ExtractFloat(block, "farClip");
        const auto cameraOrthoSize = ExtractFloat(block, "orthographicSize");
        const auto cameraPrimary = ExtractBool(block, "isPrimary", false);

        if (cameraProjection.has_value() || block.find("\"Camera\"") != std::string::npos)
        {
            auto camera = std::make_shared<CameraComponent>();
            if (cameraProjection.has_value())
            {
                camera->SetProjectionType(static_cast<CameraComponent::ProjectionType>(*cameraProjection));
            }
            if (cameraFov.has_value()) camera->SetVerticalFov(*cameraFov);
            if (cameraNear.has_value()) camera->SetNearClip(*cameraNear);
            if (cameraFar.has_value()) camera->SetFarClip(*cameraFar);
            if (cameraOrthoSize.has_value()) camera->SetOrthographicSize(*cameraOrthoSize);
            camera->SetPrimary(cameraPrimary);
            entity->AddComponent(camera);
        }

        scene->AddEntity(entity);
    }

    // Rebuild child lists after all entities are present.
    for (const auto& entity : scene->GetEntities())
    {
        if (!entity)
        {
            continue;
        }

        auto transform = entity->GetComponent<TransformComponent>();
        if (!transform || !transform->HasParent())
        {
            continue;
        }

        auto parent = scene->FindEntityById(transform->GetParentId());
        if (!parent)
        {
            transform->ClearParent();
            continue;
        }

        auto parentTransform = parent->GetComponent<TransformComponent>();
        if (!parentTransform)
        {
            transform->ClearParent();
            continue;
        }

        parentTransform->AddChild(entity->GetId());
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

    auto lightEntity = std::make_shared<Entity>(2, "Directional Light");        
    auto lightTransform = std::make_shared<TransformComponent>();
    lightTransform->SetRotationDegrees(-55.0f, 215.0f, 0.0f);
    auto light = std::make_shared<LightComponent>();
    light->SetType(LightComponent::LightType::Directional);
    light->SetPrimary(true);
    lightEntity->AddComponent(lightTransform);
    lightEntity->AddComponent(light);
    scene->AddEntity(lightEntity);

    auto cubeEntity = std::make_shared<Entity>(3, "Cube");
    auto cubeTransform = std::make_shared<TransformComponent>();
    cubeTransform->SetPosition(-2.0f, 0.0f, 0.0f);
    auto cubeMesh = std::make_shared<MeshRendererComponent>();
    cubeMesh->SetMeshAssetId("assets/meshes/cube.gltf");
    cubeEntity->AddComponent(cubeTransform);
    cubeEntity->AddComponent(cubeMesh);
    scene->AddEntity(cubeEntity);

    auto sphereEntity = std::make_shared<Entity>(4, "Sphere");
    auto sphereTransform = std::make_shared<TransformComponent>();
    sphereTransform->SetPosition(2.0f, 0.0f, 0.0f);
    auto sphereMesh = std::make_shared<MeshRendererComponent>();
    sphereMesh->SetMeshAssetId("assets/meshes/sphere.gltf");
    sphereEntity->AddComponent(sphereTransform);
    sphereEntity->AddComponent(sphereMesh);
    scene->AddEntity(sphereEntity);

    if (auto assets = m_context.GetAssetRegistry())
    {
        auto root = assets->GetRootPath();
        if (root.empty())
        {
            root = std::filesystem::current_path();
        }
        assets->Scan(root.string());
    }

    return scene;
}
} // namespace Aetherion::Scene
