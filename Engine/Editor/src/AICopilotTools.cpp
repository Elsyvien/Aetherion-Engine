#include "Aetherion/Editor/AICopilotTools.h"
#include "Aetherion/Editor/AICopilotAgent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Physics/PhysicsWorld.h"
#include "Aetherion/Scripting/LogicCopilot.h"
#include <sstream>
#include <algorithm>


namespace Aetherion::Editor {

void AICopilotToolFactory::RegisterAllTools(AICopilotAgent &agent,
                                            Scene::Scene *scene,
                                            Scene::Entity *selected,
                                            const CommandExecutor &executor,
                                            const EntityHighlightCallback &highlightCallback) {

  // Clear existing tools first
  // (agent.ClearTools() - if such method exists)

  if (!scene) {
    return;
  }

  // =====================================================================
  // list_scene_entities - List all entities in the scene
  // =====================================================================
  ToolDefinition listTool;
  listTool.name = "list_scene_entities";
  listTool.description = "Lists all entities currently in the scene with their names, IDs, and components.";
  listTool.parameters = nlohmann::json::object();
  listTool.execute = [scene](const nlohmann::json& /*params*/) -> nlohmann::json {
    nlohmann::json result;
    result["success"] = true;
    nlohmann::json entities = nlohmann::json::array();
    
    for (const auto& entity : scene->GetEntities()) {
      if (!entity) continue;
      
      nlohmann::json entityInfo;
      entityInfo["id"] = entity->GetId();
      entityInfo["name"] = entity->GetName();
      
      nlohmann::json components = nlohmann::json::array();
      if (entity->GetComponent<Scene::TransformComponent>()) {
        auto t = entity->GetComponent<Scene::TransformComponent>();
        nlohmann::json tc;
        tc["type"] = "Transform";
        tc["position"] = {{"x", t->GetPositionX()}, {"y", t->GetPositionY()}, {"z", t->GetPositionZ()}};
        tc["scale"] = {{"x", t->GetScaleX()}, {"y", t->GetScaleY()}, {"z", t->GetScaleZ()}};
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
    
    // Build human-readable summary
    std::stringstream ss;
    ss << "Scene contains " << entities.size() << " entities:\n";
    for (const auto& e : entities) {
      ss << "- " << e["name"].get<std::string>() << " (ID: " << e["id"].get<uint64_t>() << ")";
      if (!e["components"].empty()) {
        ss << " [";
        bool first = true;
        for (const auto& c : e["components"]) {
          if (!first) ss << ", ";
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

  // Register spawn_entity tool
  ToolDefinition spawnTool;
  spawnTool.name = "spawn_entity";
  spawnTool.description =
      "Creates a new entity in the scene at the specified position. Can optionally add components like colliders.";
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
      [scene, executor, highlightCallback](const nlohmann::json &params) -> nlohmann::json {
    std::string type = params.value("type", "cube");
    std::string name = params.value("name", type);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (params.contains("position")) {
      auto &pos = params["position"];
      x = pos.value("x", 0.0f);
      y = pos.value("y", 0.0f);
      z = pos.value("z", 0.0f);
    }
    
    // Gather requested components
    std::vector<std::string> requestedComponents;
    if (params.contains("components") && params["components"].is_array()) {
      for (const auto& c : params["components"]) {
        if (c.is_string()) {
          requestedComponents.push_back(c.get<std::string>());
        }
      }
    }

    // Generate unique entity ID
    Core::EntityId newId = 1;
    for (const auto& entity : scene->GetEntities()) {
      if (entity && entity->GetId() >= newId) {
        newId = entity->GetId() + 1;
      }
    }
    
    // Create the entity
    auto newEntity = std::make_shared<Scene::Entity>(newId, name);
    
    // Add transform
    auto transform = std::make_shared<Scene::TransformComponent>();
    transform->SetPosition(x, y, z);
    transform->SetScale(1.0f, 1.0f, 1.0f);
    newEntity->AddComponent(transform);
    
    // Add mesh based on type
    std::string meshId;
    if (type == "cube" || type == "box") meshId = "meshes/cube.obj";
    else if (type == "sphere") meshId = "meshes/sphere.obj";
    else if (type == "cylinder") meshId = "meshes/cylinder.obj";
    else if (type == "plane") meshId = "meshes/plane.obj";
    else if (type == "cone") meshId = "meshes/cone.obj";
    else if (type == "pyramid") meshId = "meshes/pyramid.obj";
    
    if (!meshId.empty()) {
      auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
      meshRenderer->SetMeshAssetId(meshId);
      meshRenderer->SetColor(1.0f, 1.0f, 1.0f);
      newEntity->AddComponent(meshRenderer);
    }
    
    // Add light if type is light
    if (type == "light" || type == "pointlight") {
      auto light = std::make_shared<Scene::LightComponent>();
      light->SetType(Scene::LightComponent::LightType::Point);
      light->SetIntensity(1.0f);
      light->SetColor(1.0f, 1.0f, 1.0f);
      newEntity->AddComponent(light);
    }
    
    // Add camera if type is camera
    if (type == "camera") {
      auto camera = std::make_shared<Scene::CameraComponent>();
      newEntity->AddComponent(camera);
    }
    
    // Add requested additional components
    std::vector<std::string> addedComponents;
    for (const auto& comp : requestedComponents) {
      std::string lowerComp = comp;
      std::transform(lowerComp.begin(), lowerComp.end(), lowerComp.begin(), ::tolower);
      
      if (lowerComp.find("collider") != std::string::npos || lowerComp.find("collision") != std::string::npos) {
        auto collider = std::make_shared<Scene::ColliderComponent>();
        if (type == "sphere") {
          collider->SetShapeType(Physics::ShapeType::Sphere);
        } else {
          collider->SetShapeType(Physics::ShapeType::Box);
        }
        newEntity->AddComponent(collider);
        addedComponents.push_back("Collider");
      }
      if (lowerComp.find("rigidbody") != std::string::npos || lowerComp.find("physics") != std::string::npos) {
        auto rigidbody = std::make_shared<Scene::RigidbodyComponent>();
        rigidbody->SetMass(1.0f);
        newEntity->AddComponent(rigidbody);
        addedComponents.push_back("Rigidbody");
      }
    }
    
    scene->AddEntity(newEntity);
    
    std::stringstream msg;
    msg << "Created '" << name << "' at (" << x << ", " << y << ", " << z << ")";
    if (!addedComponents.empty()) {
      msg << " with ";
      for (size_t i = 0; i < addedComponents.size(); ++i) {
        if (i > 0) msg << ", ";
        msg << addedComponents[i];
      }
    }
    
    // Highlight the newly created entity
    if (highlightCallback) {
      highlightCallback(newId, 1.5f);
    }
    
    return {{"success", true},
            {"entityId", newId},
            {"message", msg.str()}};
  };
  agent.RegisterTool(spawnTool);

  // =====================================================================
  // add_component - Add a component to an entity
  // =====================================================================
  ToolDefinition addCompTool;
  addCompTool.name = "add_component";
  addCompTool.description = "Adds a component to an existing entity by ID or name.";
  addCompTool.parameters = nlohmann::json::object(
      {{"entityId", {{"type", "number"}, {"description", "ID of the entity"}}},
       {"entityName", {{"type", "string"}, {"description", "Name of the entity (alternative to ID)"}}},
       {"component", {{"type", "string"}, {"description", "Component type: collider, rigidbody, light, camera"}}}});
  addCompTool.execute = [scene, highlightCallback](const nlohmann::json& params) -> nlohmann::json {
    std::shared_ptr<Scene::Entity> targetEntity = nullptr;
    
    // Find by ID or name
    if (params.contains("entityId")) {
      uint64_t id = params["entityId"].get<uint64_t>();
      targetEntity = scene->GetEntityById(static_cast<Core::EntityId>(id));
    }
    if (!targetEntity && params.contains("entityName")) {
      std::string name = params["entityName"].get<std::string>();
      for (const auto& e : scene->GetEntities()) {
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
    std::transform(compType.begin(), compType.end(), compType.begin(), ::tolower);
    
    if (compType.find("collider") != std::string::npos) {
      if (!targetEntity->GetComponent<Scene::ColliderComponent>()) {
        auto collider = std::make_shared<Scene::ColliderComponent>();
        collider->SetShapeType(Physics::ShapeType::Box);
        targetEntity->AddComponent(collider);
        
        if (highlightCallback) {
          highlightCallback(targetEntity->GetId(), 1.2f);
        }
        
        return {{"success", true}, {"message", "Added Collider to " + targetEntity->GetName()}};
      }
      return {{"success", false}, {"error", "Entity already has a Collider"}};
    }
    
    if (compType.find("rigidbody") != std::string::npos || compType.find("physics") != std::string::npos) {
      if (!targetEntity->GetComponent<Scene::RigidbodyComponent>()) {
        auto rb = std::make_shared<Scene::RigidbodyComponent>();
        rb->SetMass(1.0f);
        targetEntity->AddComponent(rb);
        return {{"success", true}, {"message", "Added Rigidbody to " + targetEntity->GetName()}};
      }
      return {{"success", false}, {"error", "Entity already has a Rigidbody"}};
    }
    
    if (compType.find("light") != std::string::npos) {
      if (!targetEntity->GetComponent<Scene::LightComponent>()) {
        auto light = std::make_shared<Scene::LightComponent>();
        light->SetType(Scene::LightComponent::LightType::Point);
        targetEntity->AddComponent(light);
        return {{"success", true}, {"message", "Added Light to " + targetEntity->GetName()}};
      }
      return {{"success", false}, {"error", "Entity already has a Light"}};
    }
    
    return {{"success", false}, {"error", "Unknown component type: " + compType}};
  };
  agent.RegisterTool(addCompTool);

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

  // =====================================================================
  // generate_behavior - Generate a behavior script for an entity
  // =====================================================================
  ToolDefinition behaviorTool;
  behaviorTool.name = "generate_behavior";
  behaviorTool.description = "Generates a C++ behavior script from a natural language description. "
                              "For example: 'rotate continuously' or 'move back and forth'.";
  behaviorTool.parameters = nlohmann::json::object(
      {{"description", {{"type", "string"}, {"description", "Natural language description of the behavior"}}},
       {"behaviorName", {{"type", "string"}, {"description", "Name for the behavior class (optional)"}}},
       {"entityId", {{"type", "number"}, {"description", "Entity ID to attach behavior to (optional)"}}}});
  behaviorTool.execute = [scene](const nlohmann::json& params) -> nlohmann::json {
    std::string description = params.value("description", "");
    if (description.empty()) {
      return {{"success", false}, {"error", "Missing behavior description"}};
    }
    
    std::string behaviorName = params.value("behaviorName", "");
    
    // Generate a simple behavior template based on description
    std::string lowerDesc = description;
    std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);
    
    // Auto-generate class name if not provided
    if (behaviorName.empty()) {
      if (lowerDesc.find("rotate") != std::string::npos || lowerDesc.find("spin") != std::string::npos) {
        behaviorName = "RotationBehavior";
      } else if (lowerDesc.find("move") != std::string::npos || lowerDesc.find("patrol") != std::string::npos) {
        behaviorName = "MovementBehavior";
      } else if (lowerDesc.find("bounce") != std::string::npos) {
        behaviorName = "BounceBehavior";
      } else if (lowerDesc.find("follow") != std::string::npos || lowerDesc.find("chase") != std::string::npos) {
        behaviorName = "FollowBehavior";
      } else {
        behaviorName = "CustomBehavior";
      }
    }
    
    // Build suggested code snippet
    std::stringstream headerCode;
    headerCode << "#pragma once\n";
    headerCode << "#include \"Aetherion/Scene/Component.h\"\n\n";
    headerCode << "namespace Aetherion::Scene {\n\n";
    headerCode << "/// @brief " << description << "\n";
    headerCode << "class " << behaviorName << " : public Component {\n";
    headerCode << "public:\n";
    headerCode << "    " << behaviorName << "() = default;\n";
    headerCode << "    std::string GetDisplayName() const override { return \"" << behaviorName << "\"; }\n\n";
    headerCode << "protected:\n";
    headerCode << "    void OnUpdate(float deltaTime) override;\n\n";
    headerCode << "private:\n";
    headerCode << "    float m_time{0.0f};\n";
    headerCode << "};\n\n";
    headerCode << "} // namespace Aetherion::Scene\n";
    
    std::stringstream sourceCode;
    sourceCode << "#include \"" << behaviorName << ".h\"\n";
    sourceCode << "#include \"Aetherion/Scene/Entity.h\"\n";
    sourceCode << "#include \"Aetherion/Scene/TransformComponent.h\"\n\n";
    sourceCode << "namespace Aetherion::Scene {\n\n";
    sourceCode << "void " << behaviorName << "::OnUpdate(float deltaTime) {\n";
    sourceCode << "    m_time += deltaTime;\n";
    sourceCode << "    auto* transform = GetEntity()->GetComponent<TransformComponent>().get();\n";
    sourceCode << "    if (!transform) return;\n\n";
    
    // Generate behavior-specific code
    if (lowerDesc.find("rotate") != std::string::npos || lowerDesc.find("spin") != std::string::npos) {
      sourceCode << "    // Continuous rotation\n";
      sourceCode << "    float angle = m_time * 45.0f; // 45 degrees per second\n";
      if (lowerDesc.find("x") != std::string::npos) {
        sourceCode << "    transform->SetRotation(angle, 0.0f, 0.0f);\n";
      } else if (lowerDesc.find("z") != std::string::npos) {
        sourceCode << "    transform->SetRotation(0.0f, 0.0f, angle);\n";
      } else {
        sourceCode << "    transform->SetRotation(0.0f, angle, 0.0f); // Y-axis rotation\n";
      }
    } else if (lowerDesc.find("bounce") != std::string::npos) {
      sourceCode << "    // Bouncing motion\n";
      sourceCode << "    float baseY = 0.0f;\n";
      sourceCode << "    float height = 2.0f;\n";
      sourceCode << "    float y = baseY + std::abs(std::sin(m_time * 3.0f)) * height;\n";
      sourceCode << "    transform->SetPosition(transform->GetPositionX(), y, transform->GetPositionZ());\n";
    } else if (lowerDesc.find("move") != std::string::npos || lowerDesc.find("patrol") != std::string::npos) {
      sourceCode << "    // Back and forth movement\n";
      sourceCode << "    float distance = 3.0f;\n";
      sourceCode << "    float x = std::sin(m_time) * distance;\n";
      sourceCode << "    transform->SetPosition(x, transform->GetPositionY(), transform->GetPositionZ());\n";
    } else {
      sourceCode << "    // Custom behavior - implement your logic here\n";
      sourceCode << "    // Example: oscillate on Y axis\n";
      sourceCode << "    float y = std::sin(m_time) * 0.5f;\n";
      sourceCode << "    transform->SetPosition(transform->GetPositionX(), y, transform->GetPositionZ());\n";
    }
    
    sourceCode << "}\n\n";
    sourceCode << "} // namespace Aetherion::Scene\n";
    
    nlohmann::json result;
    result["success"] = true;
    result["behaviorName"] = behaviorName;
    result["description"] = description;
    result["headerCode"] = headerCode.str();
    result["sourceCode"] = sourceCode.str();
    result["message"] = "Generated behavior script '" + behaviorName + "' for: " + description;
    result["instructions"] = "Save the header to include/Aetherion/Scene/" + behaviorName + ".h and source to src/" + behaviorName + ".cpp";
    
    return result;
  };
  agent.RegisterTool(behaviorTool);

  // =====================================================================
  // list_available_behaviors - Show available behavior templates
  // =====================================================================
  ToolDefinition listBehaviorsTool;
  listBehaviorsTool.name = "list_behaviors";
  listBehaviorsTool.description = "Lists available behavior types that can be generated.";
  listBehaviorsTool.parameters = nlohmann::json::object();
  listBehaviorsTool.execute = [](const nlohmann::json&) -> nlohmann::json {
    nlohmann::json behaviors = nlohmann::json::array();
    
    behaviors.push_back({
      {"name", "Rotation"},
      {"keywords", {"rotate", "spin", "turn"}},
      {"description", "Continuously rotates the entity around an axis"}
    });
    behaviors.push_back({
      {"name", "Movement"},
      {"keywords", {"move", "patrol", "walk"}},
      {"description", "Moves the entity back and forth or along a path"}
    });
    behaviors.push_back({
      {"name", "Bounce"},
      {"keywords", {"bounce", "jump", "hop"}},
      {"description", "Makes the entity bounce up and down"}
    });
    behaviors.push_back({
      {"name", "Follow"},
      {"keywords", {"follow", "chase", "track"}},
      {"description", "Makes the entity follow another entity or the player"}
    });
    behaviors.push_back({
      {"name", "Oscillate"},
      {"keywords", {"oscillate", "wave", "bob"}},
      {"description", "Smooth oscillation on any axis"}
    });
    
    return {
      {"success", true},
      {"behaviors", behaviors},
      {"message", "Available behaviors: Rotation, Movement, Bounce, Follow, Oscillate. Use 'generate_behavior' with a description."}
    };
  };
  agent.RegisterTool(listBehaviorsTool);

  // =====================================================================
  // modify_entity - Modify properties of an existing entity
  // =====================================================================
  ToolDefinition modifyTool;
  modifyTool.name = "modify_entity";
  modifyTool.description = "Modifies properties of an existing entity (position, scale, rotation, color).";
  modifyTool.parameters = nlohmann::json::object(
      {{"entityId", {{"type", "number"}, {"description", "ID of the entity"}}},
       {"entityName", {{"type", "string"}, {"description", "Name of the entity (alternative to ID)"}}},
       {"position", {{"type", "object"}, {"properties", {{"x", {{"type", "number"}}}, {"y", {{"type", "number"}}}, {"z", {{"type", "number"}}}}}}},
       {"scale", {{"type", "object"}, {"properties", {{"x", {{"type", "number"}}}, {"y", {{"type", "number"}}}, {"z", {{"type", "number"}}}}}}},
       {"color", {{"type", "object"}, {"properties", {{"r", {{"type", "number"}}}, {"g", {{"type", "number"}}}, {"b", {{"type", "number"}}}}}}}});
  modifyTool.execute = [scene, highlightCallback](const nlohmann::json& params) -> nlohmann::json {
    std::shared_ptr<Scene::Entity> targetEntity = nullptr;
    
    // Find by ID or name
    if (params.contains("entityId")) {
      uint64_t id = params["entityId"].get<uint64_t>();
      targetEntity = scene->GetEntityById(static_cast<Core::EntityId>(id));
    }
    if (!targetEntity && params.contains("entityName")) {
      std::string name = params["entityName"].get<std::string>();
      for (const auto& e : scene->GetEntities()) {
        if (e && e->GetName() == name) {
          targetEntity = e;
          break;
        }
      }
    }
    
    if (!targetEntity) {
      return {{"success", false}, {"error", "Entity not found"}};
    }
    
    std::vector<std::string> changes;
    
    // Modify transform
    auto transform = targetEntity->GetComponent<Scene::TransformComponent>();
    if (transform) {
      if (params.contains("position")) {
        auto& pos = params["position"];
        float x = pos.value("x", transform->GetPositionX());
        float y = pos.value("y", transform->GetPositionY());
        float z = pos.value("z", transform->GetPositionZ());
        transform->SetPosition(x, y, z);
        changes.push_back("position");
      }
      if (params.contains("scale")) {
        auto& sc = params["scale"];
        float x = sc.value("x", transform->GetScaleX());
        float y = sc.value("y", transform->GetScaleY());
        float z = sc.value("z", transform->GetScaleZ());
        transform->SetScale(x, y, z);
        changes.push_back("scale");
      }
    }
    
    // Modify mesh renderer color
    auto meshRenderer = targetEntity->GetComponent<Scene::MeshRendererComponent>();
    if (meshRenderer && params.contains("color")) {
      auto& col = params["color"];
      float r = col.value("r", 1.0f);
      float g = col.value("g", 1.0f);
      float b = col.value("b", 1.0f);
      meshRenderer->SetColor(r, g, b);
      changes.push_back("color");
    }
    
    if (changes.empty()) {
      return {{"success", false}, {"error", "No valid properties to modify"}};
    }
    
    std::stringstream msg;
    msg << "Modified " << targetEntity->GetName() << ": ";
    for (size_t i = 0; i < changes.size(); ++i) {
      if (i > 0) msg << ", ";
      msg << changes[i];
    }
    
    // Highlight the modified entity
    if (highlightCallback) {
      highlightCallback(targetEntity->GetId(), 1.5f);
    }
    
    return {{"success", true}, {"message", msg.str()}};
  };
  agent.RegisterTool(modifyTool);

  // =====================================================================
  // get_behavior_template - Get a code template for behavior scripts
  // =====================================================================
  ToolDefinition getBehaviorTemplateTool;
  getBehaviorTemplateTool.name = "get_behavior_template";
  getBehaviorTemplateTool.description = "Gets a complete C++ template for creating entity behavior scripts. "
                                         "Types: rotation, movement, animation, ai_patrol, physics_based";
  getBehaviorTemplateTool.parameters = nlohmann::json::object(
      {{"templateType", {{"type", "string"}, {"description", "Template type to retrieve"}}}});
  getBehaviorTemplateTool.execute = [](const nlohmann::json& params) -> nlohmann::json {
    std::string type = params.value("templateType", "rotation");
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    
    nlohmann::json result;
    result["success"] = true;
    
    if (type.find("rotation") != std::string::npos) {
      result["template"] = R"(
class RotationBehavior : public Component {
protected:
  void OnUpdate(float deltaTime) override {
    auto* transform = GetEntity()->GetComponent<TransformComponent>().get();
    if (!transform) return;
    
    static float angle = 0.0f;
    angle += 45.0f * deltaTime; // 45 degrees per second
    transform->SetRotation(0.0f, angle, 0.0f);
  }
};
)";
      result["description"] = "Continuously rotates entity around Y axis";
    } else if (type.find("movement") != std::string::npos) {
      result["template"] = R"(
class MovementBehavior : public Component {
protected:
  void OnUpdate(float deltaTime) override {
    auto* transform = GetEntity()->GetComponent<TransformComponent>().get();
    if (!transform) return;
    
    static float time = 0.0f;
    time += deltaTime;
    float x = std::sin(time) * 3.0f;
    transform->SetPosition(x, transform->GetPositionY(), transform->GetPositionZ());
  }
};
)";
      result["description"] = "Moves entity back and forth smoothly";
    } else if (type.find("animation") != std::string::npos) {
      result["template"] = R"(
class AnimationBehavior : public Component {
private:
  float m_time{0.0f};
  float m_animationSpeed{1.0f};
  
protected:
  void OnUpdate(float deltaTime) override {
    m_time += deltaTime * m_animationSpeed;
    auto* transform = GetEntity()->GetComponent<TransformComponent>().get();
    if (!transform) return;
    
    float scale = 1.0f + std::sin(m_time) * 0.2f;
    transform->SetScale(scale, scale, scale);
  }
};
)";
      result["description"] = "Animates entity scale with smooth bobbing";
    } else if (type.find("patrol") != std::string::npos || type.find("ai") != std::string::npos) {
      result["template"] = R"(
class AIPatrolBehavior : public Component {
private:
  enum class PatrolState { Idle, Moving, Rotating };
  PatrolState m_state{PatrolState::Idle};
  std::vector<glm::vec3> m_waypoints;
  size_t m_currentWaypoint{0};
  
protected:
  void OnUpdate(float deltaTime) override {
    auto* transform = GetEntity()->GetComponent<TransformComponent>().get();
    if (!transform || m_waypoints.empty()) return;
    
    const glm::vec3& target = m_waypoints[m_currentWaypoint];
    glm::vec3 current(transform->GetPositionX(), transform->GetPositionY(), transform->GetPositionZ());
    glm::vec3 direction = glm::normalize(target - current);
    
    float distance = glm::distance(current, target);
    if (distance < 0.1f) {
      m_currentWaypoint = (m_currentWaypoint + 1) % m_waypoints.size();
    } else {
      glm::vec3 newPos = current + direction * 2.0f * deltaTime;
      transform->SetPosition(newPos.x, newPos.y, newPos.z);
    }
  }
};
)";
      result["description"] = "AI patrol behavior with waypoint following";
    } else {
      result["template"] = R"(
class CustomBehavior : public Component {
protected:
  void OnUpdate(float deltaTime) override {
    // Implement your custom behavior here
    auto* transform = GetEntity()->GetComponent<TransformComponent>().get();
    if (!transform) return;
  }
};
)";
      result["description"] = "Basic empty template for custom behavior";
    }
    
    result["message"] = "Template retrieved: " + result["description"].get<std::string>();
    result["instructions"] = "Save to Engine/Scene/include/Aetherion/Scene/YourBehavior.h and implement in Engine/Scene/src/YourBehavior.cpp";
    
    return result;
  };
  agent.RegisterTool(getBehaviorTemplateTool);

  // =====================================================================
  // list_script_types - List available script types that can be created
  // =====================================================================
  ToolDefinition listScriptsTool;
  listScriptsTool.name = "list_script_types";
  listScriptsTool.description = "Lists all available script/behavior types that can be generated or templated.";
  listScriptsTool.parameters = nlohmann::json::object();
  listScriptsTool.execute = [](const nlohmann::json&) -> nlohmann::json {
    nlohmann::json scriptTypes = nlohmann::json::array();
    
    scriptTypes.push_back({
      {"name", "Component"},
      {"subtypes", {"Behavior", "Animation", "Physics", "Audio"}},
      {"description", "Scene component with custom Update logic"}
    });
    scriptTypes.push_back({
      {"name", "System"},
      {"subtypes", {"Render", "Physics", "Audio", "AI"}},
      {"description", "Runtime system for processing entities"}
    });
    scriptTypes.push_back({
      {"name", "Behavior"},
      {"subtypes", {"Rotation", "Movement", "AIPatrol", "Animation"}},
      {"description", "Specific behavior templates for entities"}
    });
    
    return {
      {"success", true},
      {"scriptTypes", scriptTypes},
      {"message", "Available script types. Use 'generate_behavior' or 'get_behavior_template' to create code."}
    };
  };
  agent.RegisterTool(listScriptsTool);

} // namespace Aetherion::Editor
