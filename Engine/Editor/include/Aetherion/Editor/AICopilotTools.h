#pragma once

#include "Aetherion/Editor/AICopilotAgent.h"
#include <functional>
#include <memory>

namespace Aetherion::Scene {
class Scene;
class Entity;
} // namespace Aetherion::Scene

namespace Aetherion::Editor {

// Forward declaration for command executor type
class Command;
using CommandExecutor = std::function<void(std::unique_ptr<Command>)>;

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
   * @param executor Command executor for undo/redo support (can be null)
   */
  static void RegisterAllTools(AICopilotAgent &agent, Scene::Scene *scene,
                               Scene::Entity *selected,
                               const CommandExecutor &executor);
};

} // namespace Aetherion::Editor
