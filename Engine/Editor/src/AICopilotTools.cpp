#include "Aetherion/Editor/AICopilotTools.h"
#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Editor/AICopilotAgent.h"
#include "Aetherion/Physics/PhysicsWorld.h"
#include "Aetherion/Scene/AIBehaviorComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/ScriptComponent.h"
#include "Aetherion/Scene/TransformComponent.h"
#include <algorithm>
#include <sstream>

namespace Aetherion::Editor {

void AICopilotToolFactory::RegisterAllTools(
    AICopilotAgent &agent, Scene::Scene *scene, Scene::Entity *selected,
    std::shared_ptr<Assets::AssetRegistry> assetRegistry,
    const CommandExecutor &executor,
    const EntityHighlightCallback &highlightCallback,
    const ActivityCallback &activityCallback,
    const ToolStatusCallback &toolStatusCallback) {

  if (!scene) {
    return;
  }

  // Helper to report activity
  auto reportActivity = [activityCallback](ActivityType type,
                                           const std::string &details) {
    if (activityCallback) {
      activityCallback(static_cast<int>(type), details);
    }
  };

  auto reportTool = [toolStatusCallback](const std::string &name,
                                         const std::string &params) {
    if (toolStatusCallback) {
      toolStatusCallback(name, params);
    }
  };

  // =====================================================================
  // list_scene_entities - List all entities in the scene
  // =====================================================================
  ToolDefinition listTool;
  listTool.name = "list_scene_entities";
  listTool.description = "Lists all entities currently in the scene with their "
                         "names, IDs, and components.";
  listTool.parameters = nlohmann::json::object();
  listTool.execute = [scene, reportActivity, reportTool](
                         const nlohmann::json & /*params*/) -> nlohmann::json {
    reportTool("list_scene_entities", "Scanning scene...");
    reportActivity(ActivityType::ExecutingTool, "Listing scene entities");

    nlohmann::json result;
    result["success"] = true;
    nlohmann::json entities = nlohmann::json::array();

    for (const auto &entity : scene->GetEntities()) {
      if (!entity)
        continue;

      nlohmann::json entityInfo;
      entityInfo["id"] = entity->GetId();
      entityInfo["name"] = entity->GetName();

      nlohmann::json components = nlohmann::json::array();
      if (entity->GetComponent<Scene::TransformComponent>()) {
        auto t = entity->GetComponent<Scene::TransformComponent>();
        nlohmann::json tc;
        tc["type"] = "Transform";
        tc["position"] = {{"x", t->GetPositionX()},
                          {"y", t->GetPositionY()},
                          {"z", t->GetPositionZ()}};
        tc["scale"] = {{"x", t->GetScaleX()},
                       {"y", t->GetScaleY()},
                       {"z", t->GetScaleZ()}};
        components.push_back(tc);
      }
      if (entity->GetComponent<Scene::MeshRendererComponent>()) {
        auto m = entity->GetComponent<Scene::MeshRendererComponent>();
        nlohmann::json mc;
        mc["type"] = "MeshRenderer";
        mc["mesh"] = m->GetMeshAssetId();
        components.push_back(mc);
      }
      if (entity->GetComponent<Scene::LightComponent>()) {
        auto l = entity->GetComponent<Scene::LightComponent>();
        nlohmann::json lc;
        lc["type"] = "Light";
        lc["intensity"] = l->GetIntensity();
        components.push_back(lc);
      }
      if (entity->GetComponent<Scene::CameraComponent>()) {
        nlohmann::json cc;
        cc["type"] = "Camera";
        components.push_back(cc);
      }
      if (entity->GetComponent<Scene::RigidbodyComponent>()) {
        nlohmann::json rc;
        rc["type"] = "Rigidbody";
        components.push_back(rc);
      }
      if (entity->GetComponent<Scene::ColliderComponent>()) {
        auto c = entity->GetComponent<Scene::ColliderComponent>();
        nlohmann::json cc;
        cc["type"] = "Collider";
        cc["shapeType"] = static_cast<int>(c->GetShapeType());
        components.push_back(cc);
      }

      entityInfo["components"] = components;
      entities.push_back(entityInfo);
    }

    result["entities"] = entities;
    result["count"] = entities.size();

    std::stringstream ss;
    ss << "Scene contains " << entities.size() << " entities:\n";
    for (const auto &e : entities) {
      ss << "- " << e["name"].get<std::string>()
         << " (ID: " << e["id"].get<uint64_t>() << ")";
      if (!e["components"].empty()) {
        ss << " [";
        bool first = true;
        for (const auto &c : e["components"]) {
          if (!first)
            ss << ", ";
          ss << c["type"].get<std::string>();
          first = false;
        }
        ss << "]";
      }
      ss << "\n";
    }
    result["summary"] = ss.str();

    return result;
  };
  agent.RegisterTool(listTool);

  // =====================================================================
  // spawn_entity - Create a new entity
  // =====================================================================
  ToolDefinition spawnTool;
  spawnTool.name = "spawn_entity";
  spawnTool.description =
      "Creates a new entity in the scene at the specified position. Can "
      "optionally add components like colliders.";
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
           {"z", {{"type", "number"}}}}}}},
       {"components",
        {{"type", "array"},
         {"description", "Additional components to add: collider, rigidbody"},
         {"items", {{"type", "string"}}}}}});
  spawnTool.execute =
      [scene, executor, highlightCallback, reportActivity,
       reportTool](const nlohmann::json &params) -> nlohmann::json {
    std::string type = params.value("type", "cube");
    std::string name = params.value("name", type);

    reportTool("spawn_entity", "type=" + type + ", name=" + name);
    reportActivity(ActivityType::ModifyingScene, "Creating " + name);

    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (params.contains("position")) {
      auto &pos = params["position"];
      x = pos.value("x", 0.0f);
      y = pos.value("y", 0.0f);
      z = pos.value("z", 0.0f);
    }

    std::vector<std::string> requestedComponents;
    if (params.contains("components") && params["components"].is_array()) {
      for (const auto &c : params["components"]) {
        if (c.is_string()) {
          requestedComponents.push_back(c.get<std::string>());
        }
      }
    }

    Core::EntityId newId = 1;
    for (const auto &entity : scene->GetEntities()) {
      if (entity && entity->GetId() >= newId) {
        newId = entity->GetId() + 1;
      }
    }

    auto newEntity = std::make_shared<Scene::Entity>(newId, name);

    auto transform = std::make_shared<Scene::TransformComponent>();
    transform->SetPosition(x, y, z);
    transform->SetScale(1.0f, 1.0f, 1.0f);
    newEntity->AddComponent(transform);

    std::string meshId;
    if (type == "cube" || type == "box")
      meshId = "meshes/cube.obj";
    else if (type == "sphere")
      meshId = "meshes/sphere.obj";
    else if (type == "cylinder")
      meshId = "meshes/cylinder.obj";
    else if (type == "plane")
      meshId = "meshes/plane.obj";
    else if (type == "cone")
      meshId = "meshes/cone.obj";
    else if (type == "pyramid")
      meshId = "meshes/pyramid.obj";

    if (!meshId.empty()) {
      auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
      meshRenderer->SetMeshAssetId(meshId);
      meshRenderer->SetColor(1.0f, 1.0f, 1.0f);
      newEntity->AddComponent(meshRenderer);
    }

    if (type == "light" || type == "pointlight") {
      auto light = std::make_shared<Scene::LightComponent>();
      light->SetType(Scene::LightComponent::LightType::Point);
      light->SetIntensity(1.0f);
      light->SetColor(1.0f, 1.0f, 1.0f);
      newEntity->AddComponent(light);
    }

    if (type == "camera") {
      auto camera = std::make_shared<Scene::CameraComponent>();
      newEntity->AddComponent(camera);
    }

    std::vector<std::string> addedComponents;
    for (const auto &comp : requestedComponents) {
      std::string lowerComp = comp;
      std::transform(lowerComp.begin(), lowerComp.end(), lowerComp.begin(),
                     ::tolower);

      if (lowerComp.find("collider") != std::string::npos ||
          lowerComp.find("collision") != std::string::npos) {
        auto collider = std::make_shared<Scene::ColliderComponent>();
        if (type == "sphere") {
          collider->SetShapeType(Physics::ShapeType::Sphere);
        } else {
          collider->SetShapeType(Physics::ShapeType::Box);
        }
        newEntity->AddComponent(collider);
        addedComponents.push_back("Collider");
      }
      if (lowerComp.find("rigidbody") != std::string::npos ||
          lowerComp.find("physics") != std::string::npos) {
        auto rigidbody = std::make_shared<Scene::RigidbodyComponent>();
        rigidbody->SetMass(1.0f);
        newEntity->AddComponent(rigidbody);
        addedComponents.push_back("Rigidbody");
      }
    }

    scene->AddEntity(newEntity);

    std::stringstream msg;
    msg << "Created '" << name << "' at (" << x << ", " << y << ", " << z
        << ")";
    if (!addedComponents.empty()) {
      msg << " with ";
      for (size_t i = 0; i < addedComponents.size(); ++i) {
        if (i > 0)
          msg << ", ";
        msg << addedComponents[i];
      }
    }

    if (highlightCallback) {
      highlightCallback(newId, 1.5f);
    }

    return {{"success", true}, {"entityId", newId}, {"message", msg.str()}};
  };
  agent.RegisterTool(spawnTool);

  // =====================================================================
  // add_component - Add a component to an entity
  // =====================================================================
  ToolDefinition addCompTool;
  addCompTool.name = "add_component";
  addCompTool.description =
      "Adds a component to an existing entity by ID or name.";
  addCompTool.parameters = nlohmann::json::object(
      {{"entityId", {{"type", "number"}, {"description", "ID of the entity"}}},
       {"entityName",
        {{"type", "string"},
         {"description", "Name of the entity (alternative to ID)"}}},
       {"component",
        {{"type", "string"},
         {"description",
          "Component type: collider, rigidbody, light, camera"}}}});
  addCompTool.execute = [scene, highlightCallback](
                            const nlohmann::json &params) -> nlohmann::json {
    std::shared_ptr<Scene::Entity> targetEntity = nullptr;

    if (params.contains("entityId")) {
      uint64_t id = params["entityId"].get<uint64_t>();
      targetEntity = scene->GetEntityById(static_cast<Core::EntityId>(id));
    }
    if (!targetEntity && params.contains("entityName")) {
      std::string name = params["entityName"].get<std::string>();
      for (const auto &e : scene->GetEntities()) {
        if (e && e->GetName() == name) {
          targetEntity = e;
          break;
        }
      }
    }

    if (!targetEntity) {
      return {{"success", false}, {"error", "Entity not found"}};
    }

    std::string compType = params.value("component", "");
    std::transform(compType.begin(), compType.end(), compType.begin(),
                   ::tolower);

    if (compType.find("collider") != std::string::npos) {
      if (!targetEntity->GetComponent<Scene::ColliderComponent>()) {
        auto collider = std::make_shared<Scene::ColliderComponent>();
        collider->SetShapeType(Physics::ShapeType::Box);
        targetEntity->AddComponent(collider);

        if (highlightCallback) {
          highlightCallback(targetEntity->GetId(), 1.2f);
        }

        return {{"success", true},
                {"message", "Added Collider to " + targetEntity->GetName()}};
      }
      return {{"success", false}, {"error", "Entity already has a Collider"}};
    }

    if (compType.find("rigidbody") != std::string::npos ||
        compType.find("physics") != std::string::npos) {
      if (!targetEntity->GetComponent<Scene::RigidbodyComponent>()) {
        auto rb = std::make_shared<Scene::RigidbodyComponent>();
        rb->SetMass(1.0f);
        targetEntity->AddComponent(rb);
        return {{"success", true},
                {"message", "Added Rigidbody to " + targetEntity->GetName()}};
      }
      return {{"success", false}, {"error", "Entity already has a Rigidbody"}};
    }

    if (compType.find("light") != std::string::npos) {
      if (!targetEntity->GetComponent<Scene::LightComponent>()) {
        auto light = std::make_shared<Scene::LightComponent>();
        light->SetType(Scene::LightComponent::LightType::Point);
        targetEntity->AddComponent(light);
        return {{"success", true},
                {"message", "Added Light to " + targetEntity->GetName()}};
      }
      return {{"success", false}, {"error", "Entity already has a Light"}};
    }

    return {{"success", false},
            {"error", "Unknown component type: " + compType}};
  };
  agent.RegisterTool(addCompTool);

  // =====================================================================
  // modify_entity - Modify properties of an entity
  // =====================================================================
  ToolDefinition modifyTool;
  modifyTool.name = "modify_entity";
  modifyTool.description =
      "Modifies properties of an existing entity (position, scale, color).";
  modifyTool.parameters = nlohmann::json::object(
      {{"entityId", {{"type", "number"}, {"description", "ID of the entity"}}},
       {"entityName",
        {{"type", "string"},
         {"description", "Name of the entity (alternative to ID)"}}},
       {"position",
        {{"type", "object"},
         {"properties",
          {{"x", {{"type", "number"}}},
           {"y", {{"type", "number"}}},
           {"z", {{"type", "number"}}}}}}},
       {"scale",
        {{"type", "object"},
         {"properties",
          {{"x", {{"type", "number"}}},
           {"y", {{"type", "number"}}},
           {"z", {{"type", "number"}}}}}}},
       {"color",
        {{"type", "object"},
         {"properties",
          {{"r", {{"type", "number"}}},
           {"g", {{"type", "number"}}},
           {"b", {{"type", "number"}}}}}}},
       {"rotationSpeedDegPerSec",
        {{"type", "number"},
         {"description", "Continuous spin speed (MeshRenderer) in degrees/sec. "
                         "Use 0 to stop."}}},
       {"rotationSpeed",
        {{"type", "number"},
         {"description", "Alias for rotationSpeedDegPerSec."}}}});
  modifyTool.execute = [scene, selected, highlightCallback](
                           const nlohmann::json &params) -> nlohmann::json {
    std::shared_ptr<Scene::Entity> targetEntity = nullptr;

    if (params.contains("entityId")) {
      uint64_t id = params["entityId"].get<uint64_t>();
      targetEntity = scene->GetEntityById(static_cast<Core::EntityId>(id));
    }
    if (!targetEntity && params.contains("entityName")) {
      std::string name = params["entityName"].get<std::string>();
      for (const auto &e : scene->GetEntities()) {
        if (e && e->GetName() == name) {
          targetEntity = e;
          break;
        }
      }
    }

    if (!targetEntity && !params.contains("entityId") &&
        !params.contains("entityName") && selected) {
      targetEntity =
          scene->GetEntityById(static_cast<Core::EntityId>(selected->GetId()));
    }

    if (!targetEntity) {
      return {{"success", false}, {"error", "Entity not found"}};
    }

    std::vector<std::string> changes;

    auto transform = targetEntity->GetComponent<Scene::TransformComponent>();
    if (transform) {
      if (params.contains("position")) {
        auto &pos = params["position"];
        float x = pos.value("x", transform->GetPositionX());
        float y = pos.value("y", transform->GetPositionY());
        float z = pos.value("z", transform->GetPositionZ());
        transform->SetPosition(x, y, z);
        changes.push_back("position");
      }
      if (params.contains("scale")) {
        auto &sc = params["scale"];
        float x = sc.value("x", transform->GetScaleX());
        float y = sc.value("y", transform->GetScaleY());
        float z = sc.value("z", transform->GetScaleZ());
        transform->SetScale(x, y, z);
        changes.push_back("scale");
      }
    }

    auto meshRenderer =
        targetEntity->GetComponent<Scene::MeshRendererComponent>();
    if (meshRenderer && params.contains("color")) {
      auto &col = params["color"];
      float r = col.value("r", 1.0f);
      float g = col.value("g", 1.0f);
      float b = col.value("b", 1.0f);
      meshRenderer->SetColor(r, g, b);
      changes.push_back("color");
    }

    if (meshRenderer && (params.contains("rotationSpeedDegPerSec") ||
                         params.contains("rotationSpeed"))) {
      const float speed = params.value("rotationSpeedDegPerSec",
                                       params.value("rotationSpeed", 0.0f));
      meshRenderer->SetRotationSpeedDegPerSec(speed);
      changes.push_back("rotationSpeed");
    }

    if (changes.empty()) {
      return {{"success", false}, {"error", "No valid properties to modify"}};
    }

    std::stringstream msg;
    msg << "Modified " << targetEntity->GetName() << ": ";
    for (size_t i = 0; i < changes.size(); ++i) {
      if (i > 0)
        msg << ", ";
      msg << changes[i];
    }

    if (highlightCallback) {
      highlightCallback(targetEntity->GetId(), 1.5f);
    }

    return {{"success", true}, {"message", msg.str()}};
  };
  agent.RegisterTool(modifyTool);

  // =====================================================================
  // list_behaviors - Show available behavior templates
  // =====================================================================
  ToolDefinition listBehaviorsTool;
  listBehaviorsTool.name = "list_behaviors";
  listBehaviorsTool.description =
      "Lists available behavior types that can be generated.";
  listBehaviorsTool.parameters = nlohmann::json::object();
  listBehaviorsTool.execute = [](const nlohmann::json &) -> nlohmann::json {
    nlohmann::json behaviors = nlohmann::json::array();

    behaviors.push_back(
        {{"name", "Rotation"},
         {"keywords", {"rotate", "spin", "turn"}},
         {"description", "Continuously rotates the entity around an axis"}});
    behaviors.push_back(
        {{"name", "Movement"},
         {"keywords", {"move", "patrol", "walk"}},
         {"description", "Moves the entity back and forth or along a path"}});
    behaviors.push_back(
        {{"name", "Bounce"},
         {"keywords", {"bounce", "jump", "hop"}},
         {"description", "Makes the entity bounce up and down"}});
    behaviors.push_back(
        {{"name", "Follow"},
         {"keywords", {"follow", "chase", "track"}},
         {"description",
          "Makes the entity follow another entity or the player"}});

    return {
        {"success", true},
        {"behaviors", behaviors},
        {"message",
         "Available behaviors. Use 'generate_behavior' with a description."}};
  };
  agent.RegisterTool(listBehaviorsTool);

  // =====================================================================
  // generate_asset - Generate a new asset (texture, mesh, etc.)
  // =====================================================================
  ToolDefinition genAssetTool;
  genAssetTool.name = "generate_asset";
  genAssetTool.description =
      "Generates a new asset (texture, mesh) from a text prompt.";
  genAssetTool.parameters = nlohmann::json::object(
      {{"prompt",
        {{"type", "string"},
         {"description", "Description of what to generate"}}},
       {"type",
        {{"type", "string"}, {"description", "Asset type: texture, mesh"}}},
       {"name",
        {{"type", "string"}, {"description", "Suggested name (optional)"}}}});
  genAssetTool.execute = [assetRegistry, reportActivity, reportTool](
                             const nlohmann::json &params) -> nlohmann::json {
    if (!assetRegistry) {
      return {{"success", false}, {"error", "Asset registry not available"}};
    }

    std::string prompt = params.value("prompt", "");
    std::string typeStr = params.value("type", "texture");
    std::string name = params.value("name", "");

    if (prompt.empty()) {
      return {{"success", false}, {"error", "Prompt is required"}};
    }

    Assets::AssetRegistry::AssetType type =
        Assets::AssetRegistry::AssetType::Texture;
    if (typeStr == "mesh")
      type = Assets::AssetRegistry::AssetType::Mesh;
    else if (typeStr != "texture") {
      return {{"success", false},
              {"error", "Unsupported asset type: " + typeStr}};
    }

    reportTool("generate_asset", "prompt=" + prompt + ", type=" + typeStr);
    reportActivity(ActivityType::ExecutingTool,
                   "Requesting generation: " + prompt);

    std::string id = assetRegistry->RequestGenerativeAsset(prompt, type, name);

    return {{"success", true},
            {"assetId", id},
            {"message", "Generation started for '" + prompt + "'. ID: " + id}};
  };
  agent.RegisterTool(genAssetTool);

  // =====================================================================
  // add_ai_behavior - Add AI behavior to an entity
  // =====================================================================
  ToolDefinition addAiTool;
  addAiTool.name = "add_ai_behavior";
  addAiTool.description =
      "Adds an AI Behavior component to an entity, giving it a personality.";
  addAiTool.parameters = nlohmann::json::object(
      {{"entityId", {{"type", "number"}, {"description", "ID of the entity"}}},
       {"entityName",
        {{"type", "string"},
         {"description", "Name of the entity (alternative to ID)"}}},
       {"personality",
        {{"type", "string"},
         {"description", "Description of the AI's personality"}}},
       {"context",
        {{"type", "string"},
         {"description", "Initial context/instructions"}}}});
  addAiTool.execute = [scene, highlightCallback, reportActivity](
                          const nlohmann::json &params) -> nlohmann::json {
    std::shared_ptr<Scene::Entity> targetEntity = nullptr;

    if (params.contains("entityId")) {
      uint64_t id = params["entityId"].get<uint64_t>();
      targetEntity = scene->GetEntityById(static_cast<Core::EntityId>(id));
    }
    if (!targetEntity && params.contains("entityName")) {
      std::string name = params["entityName"].get<std::string>();
      for (const auto &e : scene->GetEntities()) {
        if (e && e->GetName() == name) {
          targetEntity = e;
          break;
        }
      }
    }

    if (!targetEntity) {
      return {{"success", false}, {"error", "Entity not found"}};
    }

    auto aiComp = targetEntity->GetComponent<Scene::AIBehaviorComponent>();
    if (!aiComp) {
      aiComp = std::make_shared<Scene::AIBehaviorComponent>();
      targetEntity->AddComponent(aiComp);
    }

    std::string personality = params.value("personality", "Helpful Assistant");
    std::string context = params.value("context", "");

    aiComp->SetPersonality(personality);
    if (!context.empty()) {
      aiComp->SetContext(context);
    }

    if (highlightCallback) {
      highlightCallback(targetEntity->GetId(), 1.5f);
    }

    reportActivity(ActivityType::ModifyingScene,
                   "Added AI Behavior to " + targetEntity->GetName());

    return {{"success", true},
            {"message", "Added AI Behavior to " + targetEntity->GetName() +
                            " with personality: " + personality}};
  };
  agent.RegisterTool(addAiTool);

  // =====================================================================
  // generate_script - Generate a Lua or C++ script from description
  // =====================================================================
  ToolDefinition genScriptTool;
  genScriptTool.name = "generate_script";
  genScriptTool.description =
      "Generates a Lua or C++ script from a natural language description. "
      "Use 'lua' for simple behaviors (rotation, movement, bounce). "
      "Use 'cpp' for complex systems or performance-critical code. "
      "Use 'auto' to let the AI decide based on complexity.";
  genScriptTool.parameters = nlohmann::json::object(
      {{"description",
        {{"type", "string"}, {"description", "What the script should do"}}},
       {"language",
        {{"type", "string"},
         {"description", "Language: lua, cpp, or auto (default: auto)"}}},
       {"targetEntity",
        {{"type", "string"},
         {"description", "Name or ID of entity to attach to (optional)"}}}});
  genScriptTool.execute = [scene, reportActivity, reportTool](
                              const nlohmann::json &params) -> nlohmann::json {
    std::string description = params.value("description", "");
    std::string language = params.value("language", "auto");
    std::string targetEntity = params.value("targetEntity", "");

    if (description.empty()) {
      return {{"success", false}, {"error", "Description is required"}};
    }

    reportTool("generate_script", "desc=" + description + ", lang=" + language);
    reportActivity(ActivityType::GeneratingCode,
                   "Generating script: " + description);

    // Determine language based on complexity keywords
    if (language == "auto") {
      // Simple behaviors -> Lua
      std::string lowerDesc = description;
      std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(),
                     ::tolower);

      bool isSimple = (lowerDesc.find("rotate") != std::string::npos ||
                       lowerDesc.find("spin") != std::string::npos ||
                       lowerDesc.find("move") != std::string::npos ||
                       lowerDesc.find("bounce") != std::string::npos ||
                       lowerDesc.find("float") != std::string::npos ||
                       lowerDesc.find("blink") != std::string::npos ||
                       lowerDesc.find("pulse") != std::string::npos);

      bool isComplex = (lowerDesc.find("system") != std::string::npos ||
                        lowerDesc.find("manage") != std::string::npos ||
                        lowerDesc.find("coordinate") != std::string::npos ||
                        lowerDesc.find("network") != std::string::npos ||
                        lowerDesc.find("physics") != std::string::npos ||
                        lowerDesc.find("pathfind") != std::string::npos);

      language = isComplex ? "cpp" : "lua";
    }

    std::string code;
    std::string filename;

    if (language == "lua") {
      // Generate Lua script template
      std::string behaviorName = "CustomBehavior";

      // Extract a name from the description
      std::string lowerDesc = description;
      if (lowerDesc.find("rotate") != std::string::npos ||
          lowerDesc.find("spin") != std::string::npos) {
        behaviorName = "RotationBehavior";
      } else if (lowerDesc.find("move") != std::string::npos ||
                 lowerDesc.find("patrol") != std::string::npos) {
        behaviorName = "MovementBehavior";
      } else if (lowerDesc.find("bounce") != std::string::npos ||
                 lowerDesc.find("jump") != std::string::npos) {
        behaviorName = "BounceBehavior";
      } else if (lowerDesc.find("float") != std::string::npos) {
        behaviorName = "FloatBehavior";
      }

      // Generate a practical Lua script based on description
      std::stringstream ss;
      ss << "-- " << behaviorName << "\n";
      ss << "-- Generated from: \"" << description << "\"\n\n";
      ss << "local " << behaviorName << " = {}\n\n";
      ss << "-- Configuration\n";
      ss << behaviorName << ".speed = 1.0\n";
      ss << behaviorName << ".time = 0\n\n";
      ss << "function " << behaviorName << ":on_create()\n";
      ss << "    aetherion.log.info(\"" << behaviorName
         << " started on entity: \" .. self.entity_name)\n";
      ss << "end\n\n";
      ss << "function " << behaviorName << ":on_update(dt)\n";
      ss << "    self.time = self.time + dt\n";
      ss << "    local entity = aetherion.getEntity(self.entity_id)\n";
      ss << "    if entity then\n";
      ss << "        local transform = entity:getTransform()\n";
      ss << "        if transform then\n";

      // Add behavior-specific code
      if (behaviorName == "RotationBehavior") {
        ss << "            local rot = transform:getRotation()\n";
        ss << "            transform:setRotation(rot.x, rot.y + self.speed * "
              "dt * 90, rot.z)\n";
      } else if (behaviorName == "MovementBehavior") {
        ss << "            local pos = transform:getPosition()\n";
        ss << "            local offset = math.sin(self.time * self.speed) * "
              "2\n";
        ss << "            transform:setPosition(pos.x + offset * dt, pos.y, "
              "pos.z)\n";
      } else if (behaviorName == "BounceBehavior") {
        ss << "            local pos = transform:getPosition()\n";
        ss << "            local bounce = math.abs(math.sin(self.time * "
              "self.speed * 3)) * 1.5\n";
        ss << "            transform:setPosition(pos.x, bounce + 0.5, pos.z)\n";
      } else {
        ss << "            -- Custom behavior logic here\n";
        ss << "            -- transform:translate(0, 0, self.speed * dt)\n";
      }

      ss << "        end\n";
      ss << "    end\n";
      ss << "end\n\n";
      ss << "function " << behaviorName << ":on_destroy()\n";
      ss << "    aetherion.log.info(\"" << behaviorName << " stopped\")\n";
      ss << "end\n\n";
      ss << "return " << behaviorName << "\n";

      code = ss.str();
      filename = behaviorName + ".lua";
    } else {
      // C++ - return suggestion to use Logic Copilot panel
      return {{"success", true},
              {"language", "cpp"},
              {"message", "For C++ code generation, please use the Logic "
                          "Copilot panel (View > Logic Copilot). "
                          "It provides full code generation with compilation "
                          "and hot-reload support."},
              {"suggestion", "Use the Logic Copilot panel for C++ generation"}};
    }

    nlohmann::json result;
    result["success"] = true;
    result["language"] = language;
    result["code"] = code;
    result["filename"] = filename;
    result["message"] = "Generated " + language + " script: " + filename;

    // If target entity specified, suggest attaching
    if (!targetEntity.empty()) {
      result["attachTo"] = targetEntity;
      result["instruction"] = "Use attach_script to add this to the entity";
    }

    return result;
  };
  agent.RegisterTool(genScriptTool);

  // =====================================================================
  // attach_script - Attach a script to an entity
  // =====================================================================
  ToolDefinition attachScriptTool;
  attachScriptTool.name = "attach_script";
  attachScriptTool.description =
      "Attaches a script (Lua file or inline code) to an entity. "
      "The entity will receive a ScriptComponent with the specified script.";
  attachScriptTool.parameters = nlohmann::json::object(
      {{"entityId", {{"type", "number"}, {"description", "ID of the entity"}}},
       {"entityName",
        {{"type", "string"},
         {"description", "Name of the entity (alternative to ID)"}}},
       {"scriptSource",
        {{"type", "string"},
         {"description", "Script file path (.lua) or inline Lua code"}}},
       {"isInline",
        {{"type", "boolean"},
         {"description",
          "True if scriptSource is inline code, not a file path"}}}});
  attachScriptTool.execute =
      [scene, highlightCallback,
       reportActivity](const nlohmann::json &params) -> nlohmann::json {
    std::shared_ptr<Scene::Entity> targetEntity = nullptr;

    if (params.contains("entityId")) {
      uint64_t id = params["entityId"].get<uint64_t>();
      targetEntity = scene->GetEntityById(static_cast<Core::EntityId>(id));
    }
    if (!targetEntity && params.contains("entityName")) {
      std::string name = params["entityName"].get<std::string>();
      for (const auto &e : scene->GetEntities()) {
        if (e && e->GetName() == name) {
          targetEntity = e;
          break;
        }
      }
    }

    if (!targetEntity) {
      return {{"success", false}, {"error", "Entity not found"}};
    }

    std::string scriptSource = params.value("scriptSource", "");
    if (scriptSource.empty()) {
      return {{"success", false}, {"error", "scriptSource is required"}};
    }

    // Get or create ScriptComponent
    auto scriptComp = targetEntity->GetComponent<Scene::ScriptComponent>();
    if (!scriptComp) {
      scriptComp = std::make_shared<Scene::ScriptComponent>();
      targetEntity->AddComponent(scriptComp);
    }

    // Set the script source
    scriptComp->SetScriptSource(scriptSource);

    if (highlightCallback) {
      highlightCallback(targetEntity->GetId(), 1.5f);
    }

    reportActivity(ActivityType::ModifyingScene,
                   "Attached script to " + targetEntity->GetName());

    return {{"success", true},
            {"message", "Attached script to " + targetEntity->GetName()},
            {"entityId", targetEntity->GetId()}};
  };
  agent.RegisterTool(attachScriptTool);

  // =====================================================================
  // list_scripts - List available script templates
  // =====================================================================
  ToolDefinition listScriptsTool;
  listScriptsTool.name = "list_scripts";
  listScriptsTool.description =
      "Lists available script templates and behaviors that can be generated.";
  listScriptsTool.parameters = nlohmann::json::object();
  listScriptsTool.execute = [](const nlohmann::json &) -> nlohmann::json {
    nlohmann::json scripts = nlohmann::json::array();

    scripts.push_back(
        {{"name", "RotationBehavior"},
         {"language", "lua"},
         {"description", "Continuously rotates the entity around an axis"},
         {"keywords", {"rotate", "spin", "turn"}}});
    scripts.push_back(
        {{"name", "MovementBehavior"},
         {"language", "lua"},
         {"description", "Moves the entity back and forth or along a path"},
         {"keywords", {"move", "patrol", "oscillate"}}});
    scripts.push_back({{"name", "BounceBehavior"},
                       {"language", "lua"},
                       {"description", "Makes the entity bounce up and down"},
                       {"keywords", {"bounce", "jump", "hop"}}});
    scripts.push_back(
        {{"name", "FloatBehavior"},
         {"language", "lua"},
         {"description", "Makes the entity float gently up and down"},
         {"keywords", {"float", "hover", "levitate"}}});
    scripts.push_back(
        {{"name", "Custom C++ System"},
         {"language", "cpp"},
         {"description", "Complex ECS system for multi-entity coordination"},
         {"keywords", {"system", "manager", "controller"}},
         {"note", "Use Logic Copilot panel for C++ generation"}});

    return {{"success", true},
            {"scripts", scripts},
            {"message",
             "Use 'generate_script' with a description to create a script. "
             "For Lua: rotation, movement, bounce. For C++: use Logic Copilot "
             "panel."}};
  };
  agent.RegisterTool(listScriptsTool);
}

} // namespace Aetherion::Editor
