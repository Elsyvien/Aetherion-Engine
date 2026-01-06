#include "Aetherion/Editor/AICopilotProcessor.h"
#include "Aetherion/Editor/AICopilotAgent.h"
#include "Aetherion/Editor/AICopilotTools.h"
#include "Aetherion/Editor/Commands/EntityCommands.h"
#include "Aetherion/Editor/Commands/TransformCommand.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Assets/AssetRegistry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>
#include <array>
#include <optional>
#include <random>

namespace Aetherion::Editor {

namespace {
    enum class SpawnType {
        Empty,
        Light,
        Camera,
        Cube,
        Sphere,
        Plane,
        Cylinder,
        Cone,
        Pyramid,
        Octahedron,
        Wedge,
        TriPrism
    };
    enum class PlanTool {
        SpawnEntity,
        MoveSelection,
        RotateSelection,
        ScaleSelection,
        DeleteSelection,
        DuplicateSelection,
        ParentSelection,
        FocusSelection,
        Unknown
    };

    enum class SpawnPattern {
        Single,
        Line,
        Grid,
        Circle,
        Random
    };

    struct CopilotPlanStep {
        PlanTool tool{PlanTool::Unknown};
        QJsonObject args;
    };

    struct CopilotPlan {
        QString summary;
        std::vector<CopilotPlanStep> steps;
    };

    bool ContainsAny(const QString& text, const QStringList& needles) {
        for (const auto& needle : needles) {
            if (text.contains(needle)) {
                return true;
            }
        }
        return false;
    }

    std::optional<float> ExtractNamedFloat(const QString& text,
                                           const QStringList& keywords) {
        for (const auto& keyword : keywords) {
            QRegularExpression regex(
                QString(R"(%1\s*([-\d]+(?:\.\d+)?))")
                    .arg(QRegularExpression::escape(keyword)),
                QRegularExpression::CaseInsensitiveOption);
            auto match = regex.match(text);
            if (match.hasMatch()) {
                return match.captured(1).toFloat();
            }
        }
        return std::nullopt;
    }

    SpawnPattern ResolvePattern(const QString& patternName, bool gridFlag,
                                int count) {
        const QString pattern = patternName.trimmed().toLower();
        if (pattern == "grid" || gridFlag) {
            return SpawnPattern::Grid;
        }
        if (pattern == "circle" || pattern == "ring") {
            return SpawnPattern::Circle;
        }
        if (pattern == "random" || pattern == "scatter") {
            return SpawnPattern::Random;
        }
        if (pattern == "line" || pattern == "row") {
            return SpawnPattern::Line;
        }
        return count > 1 ? SpawnPattern::Line : SpawnPattern::Single;
    }

    QString GetLabelName(SpawnType type) {
        switch (type) {
            case SpawnType::Light: return "Light";
            case SpawnType::Camera: return "Camera";
            case SpawnType::Cube: return "Cube";
            case SpawnType::Sphere: return "Sphere";
            case SpawnType::Plane: return "Plane";
            case SpawnType::Cylinder: return "Cylinder";
            case SpawnType::Cone: return "Cone";
            case SpawnType::Pyramid: return "Pyramid";
            case SpawnType::Octahedron: return "Octahedron";
            case SpawnType::Wedge: return "Wedge";
            case SpawnType::TriPrism: return "Tri Prism";
            default: return "Entity";
        }
    }

    QString GetLabelPlural(SpawnType type) {
        switch (type) {
            case SpawnType::Light: return "lights";
            case SpawnType::Camera: return "cameras";
            case SpawnType::Cube: return "cubes";
            case SpawnType::Sphere: return "spheres";
            case SpawnType::Plane: return "planes";
            case SpawnType::Cylinder: return "cylinders";
            case SpawnType::Cone: return "cones";
            case SpawnType::Pyramid: return "pyramids";
            case SpawnType::Octahedron: return "octahedrons";
            case SpawnType::Wedge: return "wedges";
            case SpawnType::TriPrism: return "tri prisms";
            default: return "entities";
        }
    }

    const char* GetMeshAssetId(SpawnType type) {
        switch (type) {
            case SpawnType::Cube: return "97bcefcc-34c9-2f83-7bc1-faf778ae0604";
            case SpawnType::Sphere: return "e8a9b2c3-d4e5-4f6a-8b9c-0d1e2f3a4b5c";
            case SpawnType::Plane: return "88dfaa3e-27d8-f83f-7d9a-94837418dce2";
            case SpawnType::Cylinder: return "df42ee51-25aa-4c4f-bedc-94f295672e66";
            case SpawnType::Cone: return "48a0229c-c376-4469-b3f2-a29561e13557";
            case SpawnType::Pyramid: return "4934ac0a-89e3-851e-9a92-8871edb1010b";
            case SpawnType::Octahedron: return "542f0af3-982b-4de7-8b1c-6b46009bc3ea";
            case SpawnType::Wedge: return "a0064803-b33e-4bd4-8e67-457d1946f005";
            case SpawnType::TriPrism: return "65c270d7-eea5-45a5-acbe-143d357aa839";
            default: return "";
        }
    }

    PlanTool ResolvePlanTool(const QString& rawTool) {
        const QString tool = rawTool.trimmed().toLower();
        if (tool == "spawn_entity" || tool == "spawn" || tool == "create_entity") {
            return PlanTool::SpawnEntity;
        }
        if (tool == "move_selection" || tool == "move") {
            return PlanTool::MoveSelection;
        }
        if (tool == "rotate_selection" || tool == "rotate" || tool == "turn") {
            return PlanTool::RotateSelection;
        }
        if (tool == "scale_selection" || tool == "scale" || tool == "resize") {
            return PlanTool::ScaleSelection;
        }
        if (tool == "delete_selection" || tool == "delete") {
            return PlanTool::DeleteSelection;
        }
        if (tool == "duplicate_selection" || tool == "duplicate") {
            return PlanTool::DuplicateSelection;
        }
        if (tool == "parent_selection" || tool == "parent") {
            return PlanTool::ParentSelection;
        }
        if (tool == "focus_selection" || tool == "focus") {
            return PlanTool::FocusSelection;
        }
        return PlanTool::Unknown;
    }

    QString ExtractJsonPayload(const QString& prompt) {
        const QString trimmed = prompt.trimmed();
        const QRegularExpression fencedRegex(R"(```(?:json)?\s*([\s\S]*?)\s*```)");
        QRegularExpressionMatch match = fencedRegex.match(trimmed);
        if (match.hasMatch()) {
            return match.captured(1).trimmed();
        }
        return trimmed;
    }

    std::optional<CopilotPlan> TryParseCopilotPlan(const QString& prompt) {
        const QString jsonPayload = ExtractJsonPayload(prompt);
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(jsonPayload.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return std::nullopt;
        }

        const QJsonObject root = doc.object();
        const QJsonArray stepsArray = root.value("steps").toArray();
        if (stepsArray.isEmpty()) {
            return std::nullopt;
        }

        CopilotPlan plan;
        plan.summary = root.value("summary").toString();

        for (const QJsonValue& stepValue : stepsArray) {
            if (!stepValue.isObject()) {
                continue;
            }
            const QJsonObject stepObject = stepValue.toObject();
            const QString toolName = stepObject.value("tool").toString();
            CopilotPlanStep step;
            step.tool = ResolvePlanTool(toolName);
            step.args = stepObject.value("args").toObject();
            if (step.tool == PlanTool::Unknown) {
                continue;
            }
            plan.steps.push_back(step);
        }

        if (plan.steps.empty()) {
            return std::nullopt;
        }

        return plan;
    }
}

AICopilotProcessor::AICopilotProcessor(CommandExecutor executor)
    : m_executor(std::move(executor))
{
    InitializeAgent();
}

AICopilotProcessor::~AICopilotProcessor() = default;

void AICopilotProcessor::InitializeAgent() {
    m_agent = std::make_unique<AICopilotAgent>();
    
    // Configure agent with LLM settings
    AgentConfig config;
    config.endpoint = m_llmConfig.endpoint;
    config.model = m_llmConfig.model;
    config.temperature = m_llmConfig.temperature;
    config.maxTokens = m_llmConfig.maxTokens;
    m_agent->Configure(config);
}

void AICopilotProcessor::ConfigureLLM(const CopilotLLMConfig& config) {
    m_llmConfig = config;
    if (m_agent) {
        AgentConfig agentConfig;
        agentConfig.endpoint = config.endpoint;
        agentConfig.model = config.model;
        agentConfig.temperature = config.temperature;
        agentConfig.maxTokens = config.maxTokens;
        m_agent->Configure(agentConfig);
    }
}

void AICopilotProcessor::SetLLMEnabled(bool enabled) {
    m_llmConfig.enabled = enabled;
}

void AICopilotProcessor::ClearHistory() {
    if (m_agent) {
        m_agent->ClearHistory();
    }
}

void AICopilotProcessor::SetScene(std::shared_ptr<Scene::Scene> scene) {
    m_scene = std::move(scene);
    
    // Re-register tools with updated scene context
    if (m_agent && m_scene) {
        AICopilotToolFactory::RegisterAllTools(*m_agent, m_scene.get(), m_selectedEntity.get(), m_executor, m_highlightCallback, m_activityCallback, m_toolStatusCallback);
    }
}

void AICopilotProcessor::SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry) {
    m_assetRegistry = std::move(registry);
}

void AICopilotProcessor::SetSelectedEntity(std::shared_ptr<Scene::Entity> selected) {
    m_selectedEntity = std::move(selected);
    
    // Re-register tools with updated selection context
    if (m_agent && m_scene) {
        AICopilotToolFactory::RegisterAllTools(*m_agent, m_scene.get(), m_selectedEntity.get(), m_executor, m_highlightCallback, m_activityCallback, m_toolStatusCallback);
    }
}

CopilotResult AICopilotProcessor::ProcessPrompt(const QString& prompt, bool allowDryRun) {        
    CopilotResult result;

    if (!m_scene) {
        result.response = "No active scene loaded. Create or open a scene first.";
        return result;
    }

    const QString trimmed = prompt.trimmed();
    if (trimmed.isEmpty()) return result;
    
    // Try LLM-based processing first if enabled
    if (m_llmConfig.enabled && m_agent) {
        auto llmResult = ProcessWithAgent(trimmed);
        if (!llmResult.response.isEmpty() && llmResult.usedLLM) {
            return llmResult;
        }
        // Fall through to pattern matching if LLM failed
    }

    const QString lowered = trimmed.toLower();
    result.dryRun = allowDryRun &&
                    (lowered.contains("preview") || lowered.contains("dry run"));

    auto executeSpawn = [&](SpawnType type, int count, SpawnPattern pattern,
                            float spacing, float radius, float originX,
                            float originY, float originZ) {
        count = std::clamp(count, 1, 64);
        spacing = std::max(spacing, 0.1f);
        if (radius <= 0.0f) {
            if (pattern == SpawnPattern::Circle) {
                radius = std::max(1.0f, spacing * static_cast<float>(count) / 6.28318f);
            } else if (pattern == SpawnPattern::Random) {
                radius = std::max(1.0f, spacing * static_cast<float>(count) * 0.5f);
            }
        }

        const int gridSize = (pattern == SpawnPattern::Grid)
                                 ? static_cast<int>(std::ceil(std::sqrt(count)))
                                 : 1;
        const float gridOffset =
            (pattern == SpawnPattern::Grid)
                ? (static_cast<float>(gridSize - 1) * spacing * 0.5f)
                : 0.0f;
        const float lineOffset =
            (pattern == SpawnPattern::Line)
                ? (static_cast<float>(count - 1) * spacing * 0.5f)
                : 0.0f;

        std::mt19937 rng(static_cast<unsigned int>(std::random_device{}()));
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        Core::EntityId startId = 1;
        for (const auto& entity : m_scene->GetEntities()) {
            if (entity && entity->GetId() >= startId) {
                startId = entity->GetId() + 1;
            }
        }

        for (int i = 0; i < count; ++i) {
            float x = originX, y = originY, z = originZ;

            if (pattern == SpawnPattern::Grid) {
                const int col = i % gridSize;
                const int row = i / gridSize;
                x += static_cast<float>(col) * spacing - gridOffset;
                z += static_cast<float>(row) * spacing - gridOffset;
            } else if (pattern == SpawnPattern::Circle && count > 1) {
                const float t = static_cast<float>(i) / static_cast<float>(count);
                const float angle = t * 6.28318f;
                x += std::cos(angle) * radius;
                z += std::sin(angle) * radius;
            } else if (pattern == SpawnPattern::Random) {
                const float angle = unit(rng) * 6.28318f;
                const float r = std::sqrt(unit(rng)) * radius;
                x += std::cos(angle) * r;
                z += std::sin(angle) * r;
            } else if (pattern == SpawnPattern::Line && count > 1) {
                x += static_cast<float>(i) * spacing - lineOffset;
            } else if (count > 1) {
                x += static_cast<float>(i) * spacing - lineOffset;
            }

            if (type == SpawnType::Camera) {
                z += 5.0f;
            }

            QString name = GetLabelName(type);
            if (count > 1) name += QString(" %1").arg(i + 1);

            auto newEntity = std::make_shared<Scene::Entity>(startId + i, name.toStdString());

            auto transform = std::make_shared<Scene::TransformComponent>();
            transform->SetPosition(x, y, z);

            if (type == SpawnType::Light) {
                transform->SetRotationDegrees(-50.0f, -30.0f, 0.0f);
            } else {
                transform->SetRotationDegrees(0.0f, 0.0f, 0.0f);
            }
            newEntity->AddComponent(transform);

            if (type == SpawnType::Light) {
                auto light = std::make_shared<Scene::LightComponent>();
                light->SetType(Scene::LightComponent::LightType::Directional);
                light->SetIntensity(1.0f);
                newEntity->AddComponent(light);
            }
            else if (type == SpawnType::Camera) {
                auto camera = std::make_shared<Scene::CameraComponent>();
                newEntity->AddComponent(camera);
            }
            else {
                auto mesh = std::make_shared<Scene::MeshRendererComponent>();
                const char* assetId = GetMeshAssetId(type);
                if (assetId && assetId[0] != '\0') {
                    mesh->SetMeshAssetId(assetId);
                    mesh->SetColor(1.0f, 1.0f, 1.0f);
                    newEntity->AddComponent(mesh);
                }
            }

            if (m_executor) {
                if (result.dryRun) {
                    result.previewActions.push_back(
                        QString("Would create '%1' at (%2, %3, %4)")
                            .arg(name)
                            .arg(x)
                            .arg(y)
                            .arg(z));
                } else {
                    m_executor(std::make_unique<CreateEntityCommand>(m_scene, newEntity));
                }
            }

            result.createdEntityIds.push_back(newEntity->GetId());
        }

        QString typeName = (count == 1) ? GetLabelName(type) : GetLabelPlural(type);
        if (result.dryRun) {
            result.response = QString("Dry-run: would spawn %1 %2.")
                                  .arg(count)
                                  .arg(typeName);
        } else if (pattern == SpawnPattern::Grid) {
            result.response = QString("Spawned a grid of %1 %2.").arg(count).arg(typeName);
        } else if (pattern == SpawnPattern::Circle) {
            result.response = QString("Spawned a circle of %1 %2.").arg(count).arg(typeName);
        } else if (pattern == SpawnPattern::Random) {
            result.response = QString("Spawned a scatter of %1 %2.").arg(count).arg(typeName);
        } else if (pattern == SpawnPattern::Line && count > 1) {
            result.response = QString("Spawned a line of %1 %2.").arg(count).arg(typeName);
        } else {
            result.response = QString("Spawned %1 %2.").arg(count).arg(typeName);
        }
    };

    auto executeMove = [&](const std::array<float, 3>& offset) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }

        auto transform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            result.response = "Selected entity has no transform to move.";
            return;
        }

        TransformData oldTrans{};
        oldTrans.position = transform->GetPosition();
        oldTrans.rotation = transform->GetRotationDegrees();
        oldTrans.scale = transform->GetScale();

        TransformData newTrans = oldTrans;
        newTrans.position[0] += offset[0];
        newTrans.position[1] += offset[1];
        newTrans.position[2] += offset[2];

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would move '%1' by (%2, %3, %4)")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(offset[0])
                    .arg(offset[1])
                    .arg(offset[2]));
            result.response = "Dry-run: move selection.";
            return;
        }

        if (m_executor) {
            m_executor(std::make_unique<TransformCommand>(m_selectedEntity, oldTrans, newTrans));
            result.response = QString("Moved '%1' by (%2, %3, %4).")
                                  .arg(QString::fromStdString(m_selectedEntity->GetName()))
                                  .arg(offset[0])
                                  .arg(offset[1])
                                  .arg(offset[2]);
        }
    };

    auto executeRotate = [&](const std::array<float, 3>& offset, bool absolute) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }

        auto transform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            result.response = "Selected entity has no transform to rotate.";
            return;
        }

        TransformData oldTrans{};
        oldTrans.position = transform->GetPosition();
        oldTrans.rotation = transform->GetRotationDegrees();
        oldTrans.scale = transform->GetScale();

        TransformData newTrans = oldTrans;
        if (absolute) {
            newTrans.rotation = offset;
        } else {
            newTrans.rotation[0] += offset[0];
            newTrans.rotation[1] += offset[1];
            newTrans.rotation[2] += offset[2];
        }

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would rotate '%1' by (%2, %3, %4)")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(offset[0])
                    .arg(offset[1])
                    .arg(offset[2]));
            result.response = "Dry-run: rotate selection.";
            return;
        }

        if (m_executor) {
            m_executor(std::make_unique<TransformCommand>(m_selectedEntity, oldTrans, newTrans));
            result.response = QString("Rotated '%1' by (%2, %3, %4).")
                                  .arg(QString::fromStdString(m_selectedEntity->GetName()))
                                  .arg(offset[0])
                                  .arg(offset[1])
                                  .arg(offset[2]);
        }
    };

    auto executeScale = [&](const std::array<float, 3>& scale, bool absolute) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }

        auto transform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            result.response = "Selected entity has no transform to scale.";
            return;
        }

        TransformData oldTrans{};
        oldTrans.position = transform->GetPosition();
        oldTrans.rotation = transform->GetRotationDegrees();
        oldTrans.scale = transform->GetScale();

        TransformData newTrans = oldTrans;
        if (absolute) {
            newTrans.scale = scale;
        } else {
            newTrans.scale[0] *= scale[0];
            newTrans.scale[1] *= scale[1];
            newTrans.scale[2] *= scale[2];
        }

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would scale '%1' by (%2, %3, %4)")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(scale[0])
                    .arg(scale[1])
                    .arg(scale[2]));
            result.response = "Dry-run: scale selection.";
            return;
        }

        if (m_executor) {
            m_executor(std::make_unique<TransformCommand>(m_selectedEntity, oldTrans, newTrans));
            result.response = QString("Scaled '%1' by (%2, %3, %4).")
                                  .arg(QString::fromStdString(m_selectedEntity->GetName()))
                                  .arg(scale[0])
                                  .arg(scale[1])
                                  .arg(scale[2]);
        }
    };

    auto executeDelete = [&]() {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }
        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would delete entity '%1'")
                    .arg(QString::fromStdString(m_selectedEntity->GetName())));
            result.response = "Dry-run: delete selection.";
            return;
        }
        if (m_executor) {
            m_executor(std::make_unique<DeleteEntityCommand>(m_scene, m_selectedEntity));
            result.response = QString("Deleted '%1'.")
                                  .arg(QString::fromStdString(m_selectedEntity->GetName()));
        }
    };

    auto executeDuplicate = [&]() {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }

        Core::EntityId newId = 1;
        for (const auto& entity : m_scene->GetEntities()) {
            if (entity && entity->GetId() >= newId) {
                newId = entity->GetId() + 1;
            }
        }

        std::string newName = m_selectedEntity->GetName();
        if (newName.empty()) newName = "Entity";
        newName += " Copy";
        auto clone = std::make_shared<Scene::Entity>(newId, newName);

        auto srcTransform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (srcTransform) {
            auto transform = std::make_shared<Scene::TransformComponent>();
            transform->SetPosition(srcTransform->GetPosition());
            transform->SetRotationDegrees(srcTransform->GetRotationDegrees());
            transform->SetScale(srcTransform->GetScale());
            clone->AddComponent(transform);
        }
        auto srcMesh = m_selectedEntity->GetComponent<Scene::MeshRendererComponent>();
        if (srcMesh) {
            auto mesh = std::make_shared<Scene::MeshRendererComponent>();
            mesh->SetMeshAssetId(srcMesh->GetMeshAssetId());
            const auto color = srcMesh->GetColor();
            mesh->SetColor(color[0], color[1], color[2]);
            mesh->SetVisible(srcMesh->IsVisible());
            mesh->SetRotationSpeedDegPerSec(srcMesh->GetRotationSpeedDegPerSec());
            clone->AddComponent(mesh);
        }

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would duplicate '%1' to '%2'")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(QString::fromStdString(newName)));
            result.response = "Dry-run: duplicate selection.";
            return;
        }

        if (m_executor) {
            m_executor(std::make_unique<CreateEntityCommand>(m_scene, clone));
            result.response = QString("Duplicated '%1' as '%2'.")
                                  .arg(QString::fromStdString(m_selectedEntity->GetName()))
                                  .arg(QString::fromStdString(newName));
            result.createdEntityIds.push_back(newId);
        }
    };

    auto executeParent = [&](Core::EntityId targetId) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }

        if (targetId == 0 && !result.dryRun) {
            m_scene->SetParent(m_selectedEntity->GetId(), 0);
            result.response = "Detached selection to world.";
            return;
        }

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would parent '%1' to entity id %2")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(targetId));
            result.response = "Dry-run: parent selection.";
            return;
        }

        const bool ok = m_scene->SetParent(m_selectedEntity->GetId(), targetId);
        if (ok) {
            result.response =
                QString("Parented '%1' to entity %2.")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(targetId);
        } else {
            result.response = "Parent operation failed (check target id).";
        }
    };

    auto executeFocus = [&]() {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            return;
        }
        result.requestFocus = true;
        result.response = "Focusing camera on selection.";
    };

    if (auto plan = TryParseCopilotPlan(trimmed)) {
        for (const auto& step : plan->steps) {
            switch (step.tool) {
                case PlanTool::SpawnEntity: {
                    const QString typeName = step.args.value("type").toString("entity").toLower();
                    const int count = step.args.value("count").toInt(1);
                    const bool grid = step.args.value("grid").toBool(false);
                    const QString patternName =
                        step.args.value("pattern").toString(grid ? "grid" : "");
                    const float spacing = static_cast<float>(step.args.value("spacing").toDouble(2.5));
                    const float radius = static_cast<float>(step.args.value("radius").toDouble(0.0));
                    const QJsonObject origin = step.args.value("origin").toObject();
                    const float originX = static_cast<float>(origin.value("x").toDouble(0.0));
                    const float originY = static_cast<float>(origin.value("y").toDouble(0.0));
                    const float originZ = static_cast<float>(origin.value("z").toDouble(0.0));

                    SpawnType type = SpawnType::Empty;
                    if (typeName == "light") type = SpawnType::Light;
                    else if (typeName == "camera") type = SpawnType::Camera;
                    else if (typeName == "cube" || typeName == "box") type = SpawnType::Cube;
                    else if (typeName == "sphere") type = SpawnType::Sphere;
                    else if (typeName == "plane") type = SpawnType::Plane;
                    else if (typeName == "cylinder") type = SpawnType::Cylinder;
                    else if (typeName == "cone") type = SpawnType::Cone;
                    else if (typeName == "pyramid") type = SpawnType::Pyramid;
                    else if (typeName == "octahedron") type = SpawnType::Octahedron;
                    else if (typeName == "wedge") type = SpawnType::Wedge;
                    else if (typeName == "prism" || typeName == "tri_prism") type = SpawnType::TriPrism;

                    const SpawnPattern pattern = ResolvePattern(patternName, grid, count);
                    executeSpawn(type, count, pattern, spacing, radius, originX, originY, originZ);
                    break;
                }
                case PlanTool::MoveSelection: {
                    const QJsonObject offset = step.args.value("offset").toObject();
                    std::array<float, 3> delta{
                        static_cast<float>(offset.value("x").toDouble(0.0)),
                        static_cast<float>(offset.value("y").toDouble(0.0)),
                        static_cast<float>(offset.value("z").toDouble(0.0))
                    };
                    if (delta == std::array<float, 3>{0.0f, 0.0f, 0.0f}) {
                        const float distance = static_cast<float>(step.args.value("distance").toDouble(1.0));
                        const QString direction = step.args.value("direction").toString().toLower();
                        if (direction == "up") delta = {0.0f, distance, 0.0f};
                        else if (direction == "down") delta = {0.0f, -distance, 0.0f};
                        else if (direction == "forward") delta = {0.0f, 0.0f, -distance};
                        else if (direction == "back" || direction == "backward") delta = {0.0f, 0.0f, distance};
                        else if (direction == "left") delta = {-distance, 0.0f, 0.0f};
                        else if (direction == "right") delta = {distance, 0.0f, 0.0f};
                    }
                    executeMove(delta);
                    break;
                }
                case PlanTool::RotateSelection: {
                    const QJsonObject offset = step.args.value("offset").toObject();
                    std::array<float, 3> delta{
                        static_cast<float>(offset.value("x").toDouble(0.0)),
                        static_cast<float>(offset.value("y").toDouble(0.0)),
                        static_cast<float>(offset.value("z").toDouble(0.0))
                    };
                    if (delta == std::array<float, 3>{0.0f, 0.0f, 0.0f}) {
                        const float degrees = static_cast<float>(step.args.value("degrees").toDouble(15.0));
                        const QString axis = step.args.value("axis").toString("y").toLower();
                        if (axis == "x" || axis == "pitch") delta = {degrees, 0.0f, 0.0f};
                        else if (axis == "z" || axis == "roll") delta = {0.0f, 0.0f, degrees};
                        else delta = {0.0f, degrees, 0.0f};
                    }
                    const bool absolute = step.args.value("absolute").toBool(false);
                    executeRotate(delta, absolute);
                    break;
                }
                case PlanTool::ScaleSelection: {
                    const QJsonObject scaleObj = step.args.value("scale").toObject();
                    std::array<float, 3> scale{
                        static_cast<float>(scaleObj.value("x").toDouble(0.0)),
                        static_cast<float>(scaleObj.value("y").toDouble(0.0)),
                        static_cast<float>(scaleObj.value("z").toDouble(0.0))
                    };
                    float uniform = static_cast<float>(step.args.value("uniform").toDouble(0.0));
                    if (scale == std::array<float, 3>{0.0f, 0.0f, 0.0f} && uniform > 0.0f) {
                        scale = {uniform, uniform, uniform};
                    }
                    if (scale == std::array<float, 3>{0.0f, 0.0f, 0.0f}) {
                        scale = {1.0f, 1.0f, 1.0f};
                    }
                    const bool absolute = step.args.value("absolute").toBool(false);
                    executeScale(scale, absolute);
                    break;
                }
                case PlanTool::DeleteSelection:
                    executeDelete();
                    break;
                case PlanTool::DuplicateSelection:
                    executeDuplicate();
                    break;
                case PlanTool::ParentSelection: {
                    const QJsonValue targetValue = step.args.value("target_id");
                    Core::EntityId targetId = 0;
                    if (targetValue.isDouble()) {
                        targetId = static_cast<Core::EntityId>(targetValue.toInt());
                    } else if (targetValue.isString()) {
                        if (targetValue.toString().toLower() == "root") {
                            targetId = 0;
                        }
                    }
                    executeParent(targetId);
                    break;
                }
                case PlanTool::FocusSelection:
                    executeFocus();
                    break;
                default:
                    break;
            }
        }

        if (result.response.isEmpty()) {
            result.response = plan->summary.isEmpty()
                                  ? "Executed copilot plan."
                                  : plan->summary;
        }
        return result;
    }

    // Basic intent classification
    const bool spawnRequest = lowered.contains("spawn") ||
                              lowered.contains("create") ||
                              lowered.contains("add");
    const bool deleteRequest =
        lowered.contains("delete") || lowered.contains("remove");
    const bool duplicateRequest =
        lowered.contains("duplicate") || lowered.contains("copy");
    const bool moveRequest = lowered.contains("move") ||
                             lowered.contains("offset") ||
                             lowered.contains("translate");
    const bool rotateRequest =
        lowered.contains("rotate") || lowered.contains("turn");
    const bool scaleRequest =
        lowered.contains("scale") || lowered.contains("resize");
    const bool focusRequest = lowered.contains("focus") ||
                              lowered.contains("frame");
    const bool parentRequest = lowered.contains("parent");

    if (deleteRequest && m_selectedEntity) {
        executeDelete();
        return result;
    }

    if (duplicateRequest && m_selectedEntity) {
        executeDuplicate();
        return result;
    }

    if (moveRequest && m_selectedEntity) {
        float magnitude = 1.0f;
        QRegularExpression numberRegex(R"(\b(-?\d+(\.\d+)?)\b)");
        auto matchIt = numberRegex.globalMatch(trimmed);
        if (matchIt.hasNext()) {
            magnitude = matchIt.next().captured(1).toFloat();
        }

        std::array<float, 3> offset{magnitude, 0.0f, 0.0f};
        if (lowered.contains("up"))
            offset = {0.0f, magnitude, 0.0f};
        else if (lowered.contains("down"))
            offset = {0.0f, -magnitude, 0.0f};
        else if (lowered.contains("forward"))
            offset = {0.0f, 0.0f, -magnitude};
        else if (lowered.contains("back") || lowered.contains("backward"))
            offset = {0.0f, 0.0f, magnitude};
        else if (lowered.contains("left"))
            offset = {-magnitude, 0.0f, 0.0f};
        else if (lowered.contains("right"))
            offset = {magnitude, 0.0f, 0.0f};
        executeMove(offset);
        return result;
    }

    if (rotateRequest && m_selectedEntity) {
        float degrees = 15.0f;
        QRegularExpression numberRegex(R"(\b(-?\d+(\.\d+)?)\b)");
        auto matchIt = numberRegex.globalMatch(trimmed);
        if (matchIt.hasNext()) {
            degrees = matchIt.next().captured(1).toFloat();
        }

        std::array<float, 3> delta{0.0f, degrees, 0.0f};
        if (ContainsAny(lowered, {"x axis", "x-axis", "pitch"})) {
            delta = {degrees, 0.0f, 0.0f};
        } else if (ContainsAny(lowered, {"z axis", "z-axis", "roll"})) {
            delta = {0.0f, 0.0f, degrees};
        }

        executeRotate(delta, false);
        return result;
    }

    if (scaleRequest && m_selectedEntity) {
        float value = 1.0f;
        QRegularExpression numberRegex(R"(\b(-?\d+(\.\d+)?)\b)");
        auto matchIt = numberRegex.globalMatch(trimmed);
        if (matchIt.hasNext()) {
            value = matchIt.next().captured(1).toFloat();
        }

        const bool absolute = lowered.contains(" to ");
        const bool multiply =
            ContainsAny(lowered, {" by ", " x ", " times", " multiply"});
        const std::array<float, 3> scale{value, value, value};
        executeScale(scale, absolute || !multiply);
        return result;
    }

    if (parentRequest && m_selectedEntity) {
        Core::EntityId targetId = 0;
        QRegularExpression numberRegex(R"(\b(\d+)\b)");
        auto matchIt = numberRegex.globalMatch(trimmed);
        if (matchIt.hasNext()) {
            targetId = matchIt.next().captured(1).toLongLong();
        }

        if (lowered.contains("world") || lowered.contains("root")) {
            targetId = 0;
        }

        if (targetId != 0 || lowered.contains("world") || lowered.contains("root")) {
            executeParent(targetId);
            return result;
        }
    }

    if (focusRequest && m_selectedEntity) {
        executeFocus();
        return result;
    }

    if (!spawnRequest) {
        result.response = "Try spawn/move/rotate/scale/duplicate/delete commands like 'spawn a cube', 'move selection up 2', 'rotate selection 45', 'scale selection 1.5'.";
        return result;
    }

    // Extract count
    int count = 1;
    QRegularExpression numberRegex(R"(\b(\d+)\b)");
    auto matchIt = numberRegex.globalMatch(trimmed);
    if (matchIt.hasNext()) {
        count = matchIt.next().captured(1).toInt();
    }
    count = std::clamp(count, 1, 64);

    // Identify type
    SpawnType type = SpawnType::Empty;
    if (lowered.contains("light")) type = SpawnType::Light;
    else if (lowered.contains("camera")) type = SpawnType::Camera;
    else if (lowered.contains("cube") || lowered.contains("box")) type = SpawnType::Cube;
    else if (lowered.contains("sphere")) type = SpawnType::Sphere;
    else if (lowered.contains("plane")) type = SpawnType::Plane;
    else if (lowered.contains("cylinder")) type = SpawnType::Cylinder;
    else if (lowered.contains("cone")) type = SpawnType::Cone;
    else if (lowered.contains("pyramid")) type = SpawnType::Pyramid;
    else if (lowered.contains("octahedron")) type = SpawnType::Octahedron;
    else if (lowered.contains("wedge")) type = SpawnType::Wedge;
    else if (lowered.contains("prism")) type = SpawnType::TriPrism;

    const bool grid = lowered.contains("grid");
    const QString patternName = grid ? "grid" :
        (ContainsAny(lowered, {"circle", "ring"}) ? "circle" :
            (ContainsAny(lowered, {"random", "scatter", "sprinkle"}) ? "random" :
                (ContainsAny(lowered, {"line", "row"}) ? "line" : "")));
    const float spacing =
        ExtractNamedFloat(lowered, {"spacing", "gap", "spread"}).value_or(2.5f);
    const float radius =
        ExtractNamedFloat(lowered, {"radius", "r"}).value_or(0.0f);
    const SpawnPattern pattern = ResolvePattern(patternName, grid, count);
    executeSpawn(type, count, pattern, spacing, radius, 0.0f, 0.0f, 0.0f);

    return result;
}

CopilotResult AICopilotProcessor::ProcessWithAgent(const QString& prompt) {
    CopilotResult result;
    result.usedLLM = false;
    
    if (!m_agent) {
        result.response = "AI Agent not initialized.";
        return result;
    }
    
    if (!m_scene) {
        result.response = "No active scene loaded.";
        return result;
    }
    
    // Ensure tools are registered with current context
    AICopilotToolFactory::RegisterAllTools(*m_agent, m_scene.get(), m_selectedEntity.get(), m_executor, m_highlightCallback, m_activityCallback, m_toolStatusCallback);
    
    // Process the request through the agent
    std::string response = m_agent->ProcessAgenticRequest(prompt.toStdString());
    
    if (!response.empty()) {
        result.response = QString::fromStdString(response);
        result.usedLLM = true;
    } else {
        // LLM returned empty, will fall back to pattern matching
        result.response = "";
        result.usedLLM = false;
    }
    
    return result;
}

} // namespace Aetherion::Editor
