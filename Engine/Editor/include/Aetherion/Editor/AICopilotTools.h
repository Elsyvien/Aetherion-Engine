#pragma once

#include "Aetherion/Editor/AICopilotAgent.h"
#include <functional>
#include <memory>
#include <cstdint>
#include <string>

namespace Aetherion::Scene {
class Scene;
class Entity;
} // namespace Aetherion::Scene

namespace Aetherion::Assets {
class AssetRegistry;
}

namespace Aetherion::Editor {

// Forward declaration for command executor type
class Command;
using CommandExecutor = std::function<void(std::unique_ptr<Command>)>;

// Callback for entity highlighting
using EntityHighlightCallback = std::function<void(uint64_t entityId, float duration)>;

// Callback for activity status updates (for UI feedback)
using ActivityCallback = std::function<void(int activityType, const std::string& details)>;

// Callback for current tool updates (for UI feedback)
using ToolStatusCallback = std::function<void(const std::string& toolName, const std::string& params)>;

// Activity types (matches AICopilotPanel::ActivityType)
enum class ActivityType : int {
    Idle = 0,
    Thinking = 1,
    ExecutingTool = 2,
    GeneratingCode = 3,
    HighlightingEntity = 4,
    ModifyingScene = 5,
    ReadingFile = 6,
    WritingFile = 7
};

/**
 * Factory for registering AI Copilot tools that can create/modify scene
 * entities.
 *
 * Tools are registered with the AICopilotAgent and allow it to execute actions
 * in response to user prompts.
 */
class AICopilotToolFactory {
public:
  /**
   * Register all available tools with the given agent.
   *
   * @param agent The AI agent to register tools with
   * @param scene The current scene (can be null)
   * @param selected The currently selected entity (can be null)
   * @param assetRegistry The asset registry for asset operations (can be null)
   * @param executor Command executor for undo/redo support (can be null)
   * @param highlightCallback Callback to highlight entities during AI operations
   * @param activityCallback Callback to update activity status in the UI
   * @param toolStatusCallback Callback to show current tool in the UI
   */
  static void RegisterAllTools(AICopilotAgent &agent, Scene::Scene *scene,
                               Scene::Entity *selected,
                               std::shared_ptr<Assets::AssetRegistry> assetRegistry,
                               const CommandExecutor &executor,
                               const EntityHighlightCallback &highlightCallback = nullptr,
                               const ActivityCallback &activityCallback = nullptr,
                               const ToolStatusCallback &toolStatusCallback = nullptr);
};

} // namespace Aetherion::Editor
