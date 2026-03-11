#include "Aetherion/Editor/AICopilotProcessor.h"
#include "Aetherion/Editor/AICopilotAgent.h"
#include "Aetherion/Editor/AICopilotTools.h"
#include "Aetherion/Editor/Command.h"
#include "Aetherion/Editor/Commands/CompositeCommand.h"
#include "Aetherion/Editor/Commands/EntityCommands.h"
#include "Aetherion/Editor/Commands/MeshRendererCommand.h"
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
#include <QStringList>
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
        SpinSelection,
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

    QStringList SplitClauses(const QString& text) {
        const QRegularExpression splitRegex(
            R"(\b(?:and then|then|after that)\b|[;])",
            QRegularExpression::CaseInsensitiveOption);
        QStringList parts = text.split(splitRegex, Qt::SkipEmptyParts);
        QStringList trimmedParts;
        trimmedParts.reserve(parts.size());
        for (const auto& part : parts) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) {
                trimmedParts.push_back(trimmed);
            }
        }
        return trimmedParts;
    }

    std::optional<std::array<float, 3>> ExtractVector3(const QString& text) {
        QRegularExpression atRegex(
            R"(\b(?:at|position)\s*\(?\s*([-\d]+(?:\.\d+)?)\s*,\s*([-\d]+(?:\.\d+)?)\s*,\s*([-\d]+(?:\.\d+)?)\s*\)?)",
            QRegularExpression::CaseInsensitiveOption);
        auto match = atRegex.match(text);
        if (match.hasMatch()) {
            return std::array<float, 3>{
                match.captured(1).toFloat(),
                match.captured(2).toFloat(),
                match.captured(3).toFloat()
            };
        }

        auto extractAxis = [&](const QString& axis) -> std::optional<float> {
            QRegularExpression regex(
                QString(R"(\b%1\s*[:=]\s*([-\d]+(?:\.\d+)?))").arg(axis),
                QRegularExpression::CaseInsensitiveOption);
            auto axisMatch = regex.match(text);
            if (axisMatch.hasMatch()) {
                return axisMatch.captured(1).toFloat();
            }
            return std::nullopt;
        };

        auto x = extractAxis("x");
        auto y = extractAxis("y");
        auto z = extractAxis("z");
        if (x || y || z) {
            return std::array<float, 3>{
                x.value_or(0.0f),
                y.value_or(0.0f),
                z.value_or(0.0f)
            };
        }

        return std::nullopt;
    }

    std::optional<int> ExtractGridCount(const QString& text) {
        QRegularExpression gridRegex(
            R"(\b(\d+)\s*[xX]\s*(\d+)\b)");
        auto match = gridRegex.match(text);
        if (match.hasMatch()) {
            const int rows = match.captured(1).toInt();
            const int cols = match.captured(2).toInt();
            if (rows > 0 && cols > 0) {
                return rows * cols;
            }
        }
        return std::nullopt;
    }

    int ExtractMaxCount(const QString& text) {
        int maxValue = -1;
        QRegularExpression numberRegex(R"(\b(\d+)\b)");
        auto matchIt = numberRegex.globalMatch(text);
        while (matchIt.hasNext()) {
            const auto match = matchIt.next();
            maxValue = std::max(maxValue, match.captured(1).toInt());
        }
        return maxValue;
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
        if (tool == "spin_selection" || tool == "spin") {
            return PlanTool::SpinSelection;
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
    config.apiKey = m_llmConfig.apiKey;
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
        agentConfig.apiKey = config.apiKey;
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
        AICopilotToolFactory::RegisterAllTools(*m_agent, m_scene, m_selectedEntity, m_assetRegistry, m_executor, m_highlightCallback, m_activityCallback, m_toolStatusCallback);
    }
}

void AICopilotProcessor::SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry) {
    m_assetRegistry = std::move(registry);
}

void AICopilotProcessor::SetSelectedEntity(std::shared_ptr<Scene::Entity> selected) {
    m_selectedEntity = std::move(selected);
    
    // Re-register tools with updated selection context
    if (m_agent && m_scene) {
        AICopilotToolFactory::RegisterAllTools(*m_agent, m_scene, m_selectedEntity, m_assetRegistry, m_executor, m_highlightCallback, m_activityCallback, m_toolStatusCallback);
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

    return ProcessWithPatterns(trimmed, allowDryRun);
}

CopilotResult AICopilotProcessor::ProcessWithPatterns(const QString& prompt, bool allowDryRun) {
    CopilotResult result;

    if (!m_scene) {
        result.response = "No active scene loaded. Create or open a scene first.";
        return result;
    }

    const QString trimmed = prompt.trimmed();
    if (trimmed.isEmpty()) return result;

    const QString lowered = trimmed.toLower();
    const bool explicitDryRun =
        allowDryRun &&
        (lowered.contains("preview") || lowered.contains("dry run"));
    const bool destructiveIntent = ContainsAny(
        lowered, {"delete", "remove", "erase", "destroy"});
    const bool duplicateIntent =
        ContainsAny(lowered, {"duplicate", "copy"});
    const bool spawnIntent =
        ContainsAny(lowered, {"spawn", "create", "add", "make"});
    const int maxCount = ExtractMaxCount(lowered);
    const bool bulkIntent =
        spawnIntent && maxCount >= 12;
    const bool guardrailDryRun =
        allowDryRun && (destructiveIntent || duplicateIntent || bulkIntent);
    result.dryRun = explicitDryRun || guardrailDryRun;

    std::vector<std::unique_ptr<Command>> commandBatch;
    QStringList actionSummaries;

    const std::string requestId =
        "copilot-" + std::to_string(++m_requestSequence);
    auto buildContext = [&](const QString& summary) {
        CommandContext context;
        context.source = "Copilot";
        context.summary = summary.left(180).toStdString();
        context.requestId = requestId;
        return context;
    };

    auto queueCommand = [&](std::unique_ptr<Command> cmd) {
        if (result.dryRun || !cmd) {
            return;
        }
        commandBatch.push_back(std::move(cmd));
    };

    auto addSummary = [&](const QString& summary) {
        if (!summary.isEmpty()) {
            actionSummaries.push_back(summary);
        }
    };

    auto reportActivity = [&](ActivityType type, const QString& details) {
        if (m_activityCallback) {
            m_activityCallback(static_cast<int>(type), details.toStdString());
        }
    };

    if (guardrailDryRun && !explicitDryRun) {
        addSummary("Guardrail: previewing changes before applying.");
        reportActivity(ActivityType::Thinking,
                       "Guardrail preview triggered for copilot actions");
    }

    auto executeBatch = [&](const QString& summaryText) {
        if (result.dryRun || commandBatch.empty() || !m_executor) {
            return;
        }

        QString label = summaryText.trimmed();
        if (label.isEmpty()) {
            label = "Copilot Actions";
        }

        CommandContext context = buildContext(label);
        std::string name = label.left(60).toStdString();

        if (commandBatch.size() == 1) {
            commandBatch.front()->SetContext(context);
            reportActivity(ActivityType::ModifyingScene, label);
            m_executor(std::move(commandBatch.front()));
            return;
        }

        auto batch = std::make_unique<CompositeCommand>(name,
                                                        std::move(commandBatch));
        batch->SetContext(context);
        reportActivity(ActivityType::ModifyingScene, label);
        m_executor(std::move(batch));
    };

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

            if (result.dryRun) {
                result.previewActions.push_back(
                    QString("Would create '%1' at (%2, %3, %4)")
                        .arg(name)
                        .arg(x)
                        .arg(y)
                        .arg(z));
            } else {
                queueCommand(std::make_unique<CreateEntityCommand>(m_scene, newEntity));
                result.createdEntityIds.push_back(newEntity->GetId());
            }
        }

        QString typeName = (count == 1) ? GetLabelName(type) : GetLabelPlural(type);
        QString summary;
        if (result.dryRun) {
            summary = QString("Dry-run: would spawn %1 %2.")
                          .arg(count)
                          .arg(typeName);
        } else if (pattern == SpawnPattern::Grid) {
            summary = QString("Spawned a grid of %1 %2.").arg(count).arg(typeName);
        } else if (pattern == SpawnPattern::Circle) {
            summary = QString("Spawned a circle of %1 %2.").arg(count).arg(typeName);
        } else if (pattern == SpawnPattern::Random) {
            summary = QString("Spawned a scatter of %1 %2.").arg(count).arg(typeName);
        } else if (pattern == SpawnPattern::Line && count > 1) {
            summary = QString("Spawned a line of %1 %2.").arg(count).arg(typeName);
        } else {
            summary = QString("Spawned %1 %2.").arg(count).arg(typeName);
        }
        result.response = summary;
        addSummary(summary);
    };

    auto executeMove = [&](const std::array<float, 3>& offset) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }

        auto transform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            result.response = "Selected entity has no transform to move.";
            addSummary(result.response);
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
            addSummary(result.response);
            return;
        }

        queueCommand(std::make_unique<TransformCommand>(m_selectedEntity, oldTrans, newTrans));
        result.response = QString("Moved '%1' by (%2, %3, %4).")
                              .arg(QString::fromStdString(m_selectedEntity->GetName()))
                              .arg(offset[0])
                              .arg(offset[1])
                              .arg(offset[2]);
        addSummary(result.response);
    };

    auto executeRotate = [&](const std::array<float, 3>& offset, bool absolute) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }

        auto transform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            result.response = "Selected entity has no transform to rotate.";
            addSummary(result.response);
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
            addSummary(result.response);
            return;
        }

        queueCommand(std::make_unique<TransformCommand>(m_selectedEntity, oldTrans, newTrans));
        result.response = QString("Rotated '%1' by (%2, %3, %4).")
                              .arg(QString::fromStdString(m_selectedEntity->GetName()))
                              .arg(offset[0])
                              .arg(offset[1])
                              .arg(offset[2]);
        addSummary(result.response);
    };

    auto executeScale = [&](const std::array<float, 3>& scale, bool absolute) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }

        auto transform = m_selectedEntity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            result.response = "Selected entity has no transform to scale.";
            addSummary(result.response);
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
            addSummary(result.response);
            return;
        }

        queueCommand(std::make_unique<TransformCommand>(m_selectedEntity, oldTrans, newTrans));
        result.response = QString("Scaled '%1' by (%2, %3, %4).")
                              .arg(QString::fromStdString(m_selectedEntity->GetName()))
                              .arg(scale[0])
                              .arg(scale[1])
                              .arg(scale[2]);
        addSummary(result.response);
    };

    auto executeDelete = [&]() {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }
        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would delete entity '%1'")
                    .arg(QString::fromStdString(m_selectedEntity->GetName())));
            result.response = "Dry-run: delete selection.";
            addSummary(result.response);
            return;
        }
        queueCommand(std::make_unique<DeleteEntityCommand>(m_scene, m_selectedEntity));
        result.response = QString("Deleted '%1'.")
                              .arg(QString::fromStdString(m_selectedEntity->GetName()));
        addSummary(result.response);
    };

    auto executeDuplicate = [&]() {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
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
            addSummary(result.response);
            return;
        }

        queueCommand(std::make_unique<CreateEntityCommand>(m_scene, clone));
        result.response = QString("Duplicated '%1' as '%2'.")
                              .arg(QString::fromStdString(m_selectedEntity->GetName()))
                              .arg(QString::fromStdString(newName));
        result.createdEntityIds.push_back(newId);
        addSummary(result.response);
    };

    auto executeParent = [&](Core::EntityId targetId) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would parent '%1' to entity id %2")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(targetId));
            result.response = "Dry-run: parent selection.";
            addSummary(result.response);
            return;
        }

        queueCommand(std::make_unique<ParentEntityCommand>(
            m_scene, m_selectedEntity->GetId(), targetId));
        result.response = (targetId == 0)
                              ? "Detached selection to world."
                              : QString("Parented '%1' to entity %2.")
                                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                                    .arg(targetId);
        addSummary(result.response);
    };

    auto executeSpin = [&](float speed) {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }

        auto mesh = m_selectedEntity->GetComponent<Scene::MeshRendererComponent>();
        if (!mesh) {
            result.response = "Selected entity has no mesh renderer to spin.";
            addSummary(result.response);
            return;
        }

        if (result.dryRun) {
            result.previewActions.push_back(
                QString("Would set spin speed of '%1' to %2 deg/sec")
                    .arg(QString::fromStdString(m_selectedEntity->GetName()))
                    .arg(speed));
            result.response = "Dry-run: spin selection.";
            addSummary(result.response);
            return;
        }

        MeshRendererState oldState = CaptureMeshRendererState(*mesh);
        MeshRendererState newState = oldState;
        newState.rotationSpeedDegPerSec = speed;
        queueCommand(std::make_unique<MeshRendererCommand>(
            m_selectedEntity, oldState, newState));
        result.response = QString("Set '%1' spin speed to %2 deg/sec.")
                              .arg(QString::fromStdString(m_selectedEntity->GetName()))
                              .arg(speed);
        addSummary(result.response);
    };

    auto executeFocus = [&]() {
        if (!m_selectedEntity) {
            result.response = "No entity selected.";
            addSummary(result.response);
            return;
        }
        result.requestFocus = true;
        result.response = "Focusing camera on selection.";
        addSummary(result.response);
    };

    if (auto plan = TryParseCopilotPlan(trimmed)) {
        if (!explicitDryRun && allowDryRun) {
            bool planGuardrail = false;
            for (const auto& step : plan->steps) {
                if (step.tool == PlanTool::DeleteSelection ||
                    step.tool == PlanTool::DuplicateSelection) {
                    planGuardrail = true;
                    break;
                }
                if (step.tool == PlanTool::SpawnEntity) {
                    const int count = step.args.value("count").toInt(1);
                    if (count >= 12) {
                        planGuardrail = true;
                        break;
                    }
                }
            }
            if (planGuardrail) {
                result.dryRun = true;
                addSummary("Guardrail: previewing plan before applying.");
                reportActivity(ActivityType::Thinking,
                               "Guardrail preview triggered for plan");
            }
        }
        for (const auto& step : plan->steps) {
            switch (step.tool) {
                case PlanTool::SpawnEntity: {
                    const QString typeName = step.args.value("type").toString("entity").toLower();
                    int count = step.args.value("count").toInt(1);
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
                case PlanTool::SpinSelection: {
                    float speed = static_cast<float>(step.args.value("speed").toDouble(90.0));
                    speed = static_cast<float>(
                        step.args.value("degrees_per_sec").toDouble(speed));
                    executeSpin(speed);
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

        if (!actionSummaries.isEmpty()) {
            if (plan->summary.isEmpty()) {
                result.response = actionSummaries.join("\n");
            }
        }
        if (result.response.isEmpty()) {
            result.response = plan->summary.isEmpty()
                                  ? "Executed copilot plan."
                                  : plan->summary;
        }

        executeBatch(plan->summary.isEmpty() ? result.response : plan->summary);
        return result;
    }

    auto processClause = [&](const QString& clause) -> bool {
        if (clause.trimmed().isEmpty()) {
            return false;
        }

        const QString clauseLower = clause.toLower();

        const bool addVerb = clauseLower.contains("add");
        const bool addToRequest = addVerb && clauseLower.contains(" to ");
        const bool addEntityRequest =
            addVerb && !addToRequest &&
            ContainsAny(clauseLower, {"cube", "box", "sphere", "plane", "cylinder",
                                      "cone", "pyramid", "octahedron", "wedge",
                                      "prism", "light", "camera"});
        const bool spawnRequest = clauseLower.contains("spawn") ||
                                  clauseLower.contains("create") ||
                                  addEntityRequest;
        const bool deleteRequest =
            clauseLower.contains("delete") || clauseLower.contains("remove");
        const bool duplicateRequest =
            clauseLower.contains("duplicate") || clauseLower.contains("copy");
        const bool moveRequest = clauseLower.contains("move") ||
                                 clauseLower.contains("offset") ||
                                 clauseLower.contains("translate");
        const bool rotateRequest =
            clauseLower.contains("rotate") || clauseLower.contains("turn");
        const bool spinRequest =
            clauseLower.contains("spin") || clauseLower.contains("spinning");
        const bool scaleRequest =
            clauseLower.contains("scale") || clauseLower.contains("resize");
        const bool focusRequest = clauseLower.contains("focus") ||
                                  clauseLower.contains("frame");
        const bool parentRequest = clauseLower.contains("parent") ||
                                   clauseLower.contains("attach") ||
                                   addToRequest;

        if (deleteRequest && m_selectedEntity) {
            executeDelete();
            return true;
        }

        if (duplicateRequest && m_selectedEntity) {
            executeDuplicate();
            return true;
        }

        if (moveRequest && m_selectedEntity) {
            float magnitude = 1.0f;
            QRegularExpression numberRegex(R"(\b(-?\d+(\.\d+)?)\b)");
            auto matchIt = numberRegex.globalMatch(clause);
            if (matchIt.hasNext()) {
                magnitude = matchIt.next().captured(1).toFloat();
            }

            std::array<float, 3> offset{magnitude, 0.0f, 0.0f};
            if (clauseLower.contains("up"))
                offset = {0.0f, magnitude, 0.0f};
            else if (clauseLower.contains("down"))
                offset = {0.0f, -magnitude, 0.0f};
            else if (clauseLower.contains("forward"))
                offset = {0.0f, 0.0f, -magnitude};
            else if (clauseLower.contains("back") || clauseLower.contains("backward"))
                offset = {0.0f, 0.0f, magnitude};
            else if (clauseLower.contains("left"))
                offset = {-magnitude, 0.0f, 0.0f};
            else if (clauseLower.contains("right"))
                offset = {magnitude, 0.0f, 0.0f};
            executeMove(offset);
            return true;
        }

        if (rotateRequest && m_selectedEntity) {
            float degrees = 15.0f;
            QRegularExpression numberRegex(R"(\b(-?\d+(\.\d+)?)\b)");
            auto matchIt = numberRegex.globalMatch(clause);
            if (matchIt.hasNext()) {
                degrees = matchIt.next().captured(1).toFloat();
            }

            std::array<float, 3> delta{0.0f, degrees, 0.0f};
            if (ContainsAny(clauseLower, {"x axis", "x-axis", "pitch"})) {
                delta = {degrees, 0.0f, 0.0f};
            } else if (ContainsAny(clauseLower, {"z axis", "z-axis", "roll"})) {
                delta = {0.0f, 0.0f, degrees};
            }

            executeRotate(delta, false);
            return true;
        }

        if (scaleRequest && m_selectedEntity) {
            float value = 1.0f;
            QRegularExpression numberRegex(R"(\b(-?\d+(\.\d+)?)\b)");
            auto matchIt = numberRegex.globalMatch(clause);
            if (matchIt.hasNext()) {
                value = matchIt.next().captured(1).toFloat();
            }

            const bool absolute = clauseLower.contains(" to ");
            const bool multiply =
                ContainsAny(clauseLower, {" by ", " x ", " times", " multiply"});
            const std::array<float, 3> scale{value, value, value};
            executeScale(scale, absolute || !multiply);
            return true;
        }

        if (spinRequest && m_selectedEntity) {
            float speed = 90.0f;
            QRegularExpression numberRegex(R"(\b(-?\d+(?:\.\d+)?)\b)");
            auto matchIt = numberRegex.globalMatch(clause);
            if (matchIt.hasNext()) {
                speed = matchIt.next().captured(1).toFloat();
            }
            if (ContainsAny(clauseLower, {"stop", "disable", "no spin", "not spin"})) {
                speed = 0.0f;
            }

            executeSpin(speed);
            return true;
        }

        if (parentRequest && m_selectedEntity) {
            Core::EntityId targetId = 0;
            QRegularExpression numberRegex(R"(\b(\d+)\b)");
            auto matchIt = numberRegex.globalMatch(clause);
            if (matchIt.hasNext()) {
                targetId = matchIt.next().captured(1).toLongLong();
            }

            if (clauseLower.contains("world") || clauseLower.contains("root")) {
                targetId = 0;
            }

            if (targetId != 0 || clauseLower.contains("world") || clauseLower.contains("root")) {
                executeParent(targetId);
                return true;
            }
        }

        if (focusRequest && m_selectedEntity) {
            executeFocus();
            return true;
        }

        if (!spawnRequest) {
            return false;
        }

        int count = 1;
        QRegularExpression numberRegex(R"(\b(\d+)\b)");
        auto matchIt = numberRegex.globalMatch(clause);
        if (matchIt.hasNext()) {
            count = matchIt.next().captured(1).toInt();
        }
        if (auto gridCount = ExtractGridCount(clause)) {
            count = gridCount.value();
        }
        count = std::clamp(count, 1, 64);

        SpawnType type = SpawnType::Empty;
        if (clauseLower.contains("light")) type = SpawnType::Light;
        else if (clauseLower.contains("camera")) type = SpawnType::Camera;
        else if (clauseLower.contains("cube") || clauseLower.contains("box")) type = SpawnType::Cube;
        else if (clauseLower.contains("sphere")) type = SpawnType::Sphere;
        else if (clauseLower.contains("plane")) type = SpawnType::Plane;
        else if (clauseLower.contains("cylinder")) type = SpawnType::Cylinder;
        else if (clauseLower.contains("cone")) type = SpawnType::Cone;
        else if (clauseLower.contains("pyramid")) type = SpawnType::Pyramid;
        else if (clauseLower.contains("octahedron")) type = SpawnType::Octahedron;
        else if (clauseLower.contains("wedge")) type = SpawnType::Wedge;
        else if (clauseLower.contains("prism")) type = SpawnType::TriPrism;

        const bool grid = clauseLower.contains("grid") || ExtractGridCount(clause).has_value();
        const QString patternName = grid ? "grid" :
            (ContainsAny(clauseLower, {"circle", "ring"}) ? "circle" :
                (ContainsAny(clauseLower, {"random", "scatter", "sprinkle"}) ? "random" :
                    (ContainsAny(clauseLower, {"line", "row"}) ? "line" : "")));
        const float spacing =
            ExtractNamedFloat(clauseLower, {"spacing", "gap", "spread"}).value_or(2.5f);
        const float radius =
            ExtractNamedFloat(clauseLower, {"radius", "r"}).value_or(0.0f);
        const SpawnPattern pattern = ResolvePattern(patternName, grid, count);
        float originX = 0.0f;
        float originY = 0.0f;
        float originZ = 0.0f;
        if (auto origin = ExtractVector3(clause)) {
            originX = origin->at(0);
            originY = origin->at(1);
            originZ = origin->at(2);
        }
        executeSpawn(type, count, pattern, spacing, radius, originX, originY, originZ);
        return true;
    };

    bool handled = false;
    const QStringList clauses = SplitClauses(trimmed);
    if (clauses.empty()) {
        handled = processClause(trimmed);
    } else {
        for (const auto& clause : clauses) {
            if (processClause(clause)) {
                handled = true;
            }
        }
    }

    if (!handled) {
        result.response = "Try spawn/move/rotate/scale/duplicate/delete commands like 'spawn a cube', 'move selection up 2', 'rotate selection 45', 'scale selection 1.5'.";
        return result;
    }

    if (!actionSummaries.isEmpty()) {
        if (actionSummaries.size() == 1) {
            result.response = actionSummaries.front();
        } else {
            result.response = actionSummaries.join("\n");
        }
    }

    executeBatch(result.response);
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
    AICopilotToolFactory::RegisterAllTools(*m_agent, m_scene, m_selectedEntity, m_assetRegistry, m_executor, m_highlightCallback, m_activityCallback, m_toolStatusCallback);
    
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
