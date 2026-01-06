#include "Aetherion/Editor/AICopilotTools.h"
#include "Aetherion/Editor/AICopilotAgent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"


namespace Aetherion::Editor {

void AICopilotToolFactory::RegisterAllTools(AICopilotAgent &agent,
                                            Scene::Scene *scene,
                                            Scene::Entity *selected,
                                            const CommandExecutor &executor) {

  // Clear existing tools first
  // (agent.ClearTools() - if such method exists)

  if (!scene) {
    return;
  }

  // Register spawn_entity tool
  ToolDefinition spawnTool;
  spawnTool.name = "spawn_entity";
  spawnTool.description =
      "Creates a new entity in the scene at the specified position.";
  spawnTool.parameters = nlohmann::json::object(
      {{"type",
        {{"type", "string"},
         {"description",
          "Entity type: cube, sphere, cylinder, plane, light, camera, empty"}}},
       {"name",
        {{"type", "string"},
         {"description", "Name for the new entity (optional)"}}},
       {"position",
        {{"type", "object"},
         {"properties",
          {{"x", {{"type", "number"}}},
           {"y", {{"type", "number"}}},
           {"z", {{"type", "number"}}}}}}}});
  spawnTool.execute =
      [scene, executor](const nlohmann::json &params) -> nlohmann::json {
    // Basic stub implementation - returns success
    // Full implementation would create the entity using commands
    std::string type = params.value("type", "cube");
    std::string name = params.value("name", type);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (params.contains("position")) {
      auto &pos = params["position"];
      x = pos.value("x", 0.0f);
      y = pos.value("y", 0.0f);
      z = pos.value("z", 0.0f);
    }

    // TODO: Actually create entity using executor if available
    return {{"success", true},
            {"message", "Entity '" + name + "' of type '" + type +
                            "' would be spawned at (" + std::to_string(x) +
                            ", " + std::to_string(y) + ", " +
                            std::to_string(z) + ")"},
            {"note", "Tool execution needs full implementation"}};
  };
  agent.RegisterTool(spawnTool);

  // Register move_selection tool
  if (selected) {
    ToolDefinition moveTool;
    moveTool.name = "move_selection";
    moveTool.description =
        "Moves the currently selected entity by the given offset.";
    moveTool.parameters =
        nlohmann::json::object({{"offset",
                                 {{"type", "object"},
                                  {"properties",
                                   {{"x", {{"type", "number"}}},
                                    {"y", {{"type", "number"}}},
                                    {"z", {{"type", "number"}}}}}}}});
    moveTool.execute =
        [selected](const nlohmann::json &params) -> nlohmann::json {
      float x = 0.0f, y = 0.0f, z = 0.0f;
      if (params.contains("offset")) {
        auto &off = params["offset"];
        x = off.value("x", 0.0f);
        y = off.value("y", 0.0f);
        z = off.value("z", 0.0f);
      }
      return {{"success", true},
              {"message", "Would move selection by (" + std::to_string(x) +
                              ", " + std::to_string(y) + ", " +
                              std::to_string(z) + ")"},
              {"note", "Tool execution needs full implementation"}};
    };
    agent.RegisterTool(moveTool);

    // Register delete_selection tool
    ToolDefinition deleteTool;
    deleteTool.name = "delete_selection";
    deleteTool.description = "Deletes the currently selected entity.";
    deleteTool.parameters = nlohmann::json::object();
    deleteTool.execute =
        [selected](const nlohmann::json & /* params */) -> nlohmann::json {
      return {{"success", true},
              {"message", "Would delete selection"},
              {"note", "Tool execution needs full implementation"}};
    };
    agent.RegisterTool(deleteTool);
  }
}

} // namespace Aetherion::Editor
