#include "Aetherion/Editor/AICopilotProcessor.h"
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

namespace Aetherion::Editor {

namespace {
    enum class SpawnType { Empty, Light, Camera, Cube };
    enum class PlanTool {
        SpawnEntity,
        MoveSelection,
        DeleteSelection,
        DuplicateSelection,
        ParentSelection,
        FocusSelection,
        Unknown
    };

    struct CopilotPlanStep {
        PlanTool tool{PlanTool::Unknown};
        QJsonObject args;
    };

    struct CopilotPlan {
        QString summary;
        std::vector<CopilotPlanStep> steps;
    };

    QString GetLabelName(SpawnType type) {
        switch (type) {
            case SpawnType::Light: return "Light";
            case SpawnType::Camera: return "Camera";
            case SpawnType::Cube: return "Cube";
            default: return "Entity";
        }
    }

    QString GetLabelPlural(SpawnType type) {
        switch (type) {
            case SpawnType::Light: return "lights";
            case SpawnType::Camera: return "cameras";
            case SpawnType::Cube: return "cubes";
            default: return "entities";
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
}

void AICopilotProcessor::SetScene(std::shared_ptr<Scene::Scene> scene) {
    m_scene = std::move(scene);
}

void AICopilotProcessor::SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry) {
    m_assetRegistry = std::move(registry);
}

void AICopilotProcessor::SetSelectedEntity(std::shared_ptr<Scene::Entity> selected) {
    m_selectedEntity = std::move(selected);
}

CopilotResult AICopilotProcessor::ProcessPrompt(const QString& prompt, bool allowDryRun) {        
    CopilotResult result;

    if (!m_scene) {
        result.response = "No active scene loaded. Create or open a scene first.";
        return result;
    }

    const QString trimmed = prompt.trimmed();
    if (trimmed.isEmpty()) return result;

    const QString lowered = trimmed.toLower();
    result.dryRun = allowDryRun &&
                    (lowered.contains("preview") || lowered.contains("dry run"));

    auto executeSpawn = [&](SpawnType type, int count, bool grid, float spacing,
                            float originX, float originY, float originZ) {
        count = std::clamp(count, 1, 64);
        const int gridSize = grid ? static_cast<int>(std::ceil(std::sqrt(count))) : 1;
        const float gridOffset = grid ? (static_cast<float>(gridSize - 1) * spacing * 0.5f) : 0.0f;

        Core::EntityId startId = 1;
        for (const auto& entity : m_scene->GetEntities()) {
            if (entity && entity->GetId() >= startId) {
                startId = entity->GetId() + 1;
            }
        }

        for (int i = 0; i < count; ++i) {
            float x = originX, y = originY, z = originZ;

            if (grid) {
                const int col = i % gridSize;
                const int row = i / gridSize;
                x += static_cast<float>(col) * spacing - gridOffset;
                z += static_cast<float>(row) * spacing - gridOffset;
            } else if (count > 1) {
                x += static_cast<float>(i) * spacing;
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
            else if (type == SpawnType::Cube) {
                auto mesh = std::make_shared<Scene::MeshRendererComponent>();
                mesh->SetMeshAssetId("97bcefcc-34c9-2f83-7bc1-faf778ae0604"); 
                mesh->SetColor(1.0f, 1.0f, 1.0f);
                newEntity->AddComponent(mesh);
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
        } else if (grid) {
            result.response = QString("Spawned a grid of %1 %2.").arg(count).arg(typeName);
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
                    const float spacing = static_cast<float>(step.args.value("spacing").toDouble(2.5));
                    const QJsonObject origin = step.args.value("origin").toObject();
                    const float originX = static_cast<float>(origin.value("x").toDouble(0.0));
                    const float originY = static_cast<float>(origin.value("y").toDouble(0.0));
                    const float originZ = static_cast<float>(origin.value("z").toDouble(0.0));

                    SpawnType type = SpawnType::Empty;
                    if (typeName == "light") type = SpawnType::Light;
                    else if (typeName == "camera") type = SpawnType::Camera;
                    else if (typeName == "cube" || typeName == "box") type = SpawnType::Cube;

                    executeSpawn(type, count, grid, spacing, originX, originY, originZ);
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
        result.response = "Try spawn/move/duplicate/delete commands like 'spawn a cube', 'move selection up 2', 'delete selection'.";
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

    // Grid logic
    const bool grid = lowered.contains("grid");
    const float spacing = 2.5f;
    executeSpawn(type, count, grid, spacing, 0.0f, 0.0f, 0.0f);

    return result;
}

} // namespace Aetherion::Editor
