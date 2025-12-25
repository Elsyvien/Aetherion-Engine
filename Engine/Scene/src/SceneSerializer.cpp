#include "Aetherion/Scene/SceneSerializer.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "nlohmann/json.hpp"

namespace {
using namespace Aetherion;
using Json = nlohmann::ordered_json;

float ReadNumber(const Json &obj, const char *key, float fallback) {
  try {
    return obj.value(key, fallback);
  } catch (const nlohmann::json::exception &) {
    return fallback;
  }
}

int ReadInt(const Json &obj, const char *key, int fallback) {
  try {
    return obj.value(key, fallback);
  } catch (const nlohmann::json::exception &) {
    return fallback;
  }
}

Core::EntityId ReadEntityId(const Json &obj, const char *key,
                            Core::EntityId fallback) {
  try {
    return obj.value(key, fallback);
  } catch (const nlohmann::json::exception &) {
    return fallback;
  }
}

bool ReadBool(const Json &obj, const char *key, bool fallback) {
  try {
    return obj.value(key, fallback);
  } catch (const nlohmann::json::exception &) {
    return fallback;
  }
}

std::string ReadString(const Json &obj, const char *key,
                       const std::string &fallback) {
  try {
    return obj.value(key, fallback);
  } catch (const nlohmann::json::exception &) {
    return fallback;
  }
}

bool ReadVec3(const Json &obj, const char *key, std::array<float, 3> &out,
              std::size_t minSize) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_array() || it->size() < minSize) {
    return false;
  }

  if (it->size() >= 1 && (*it)[0].is_number()) {
    out[0] = (*it)[0].get<float>();
  }
  if (it->size() >= 2 && (*it)[1].is_number()) {
    out[1] = (*it)[1].get<float>();
  }
  if (it->size() >= 3 && (*it)[2].is_number()) {
    out[2] = (*it)[2].get<float>();
  }
  return true;
}
} // namespace

namespace Aetherion::Scene {
SceneSerializer::SceneSerializer(Runtime::EngineContext &context)
    : m_context(context) {}

bool SceneSerializer::Save(const Scene &scene,
                           const std::filesystem::path &path) const {
  if (!std::filesystem::exists(path.parent_path())) {
    std::filesystem::create_directories(path.parent_path());
  }

  std::ofstream out(path);
  if (!out.is_open()) {
    return false;
  }

  Json root;
  root["name"] = scene.GetName();
  root["entities"] = Json::array();

  const auto &entities = scene.GetEntities();
  for (const auto &entity : entities) {
    if (!entity) {
      continue;
    }

    Json entityJson;
    entityJson["id"] = entity->GetId();
    entityJson["name"] = entity->GetName();

    Json components = Json::object();

    if (auto transform = entity->GetComponent<TransformComponent>()) {
      Json transformJson;
      transformJson["position"] = {transform->GetPositionX(),
                                   transform->GetPositionY(),
                                   transform->GetPositionZ()};
      transformJson["rotation"] = {transform->GetRotationXDegrees(),
                                   transform->GetRotationYDegrees(),
                                   transform->GetRotationZDegrees()};
      transformJson["scale"] = {transform->GetScaleX(), transform->GetScaleY(),
                                transform->GetScaleZ()};
      transformJson["parent"] = transform->GetParentId();
      components["Transform"] = std::move(transformJson);
    }

    if (auto mesh = entity->GetComponent<MeshRendererComponent>()) {
      Json meshJson;
      const auto color = mesh->GetColor();
      meshJson["visible"] = mesh->IsVisible();
      meshJson["color"] = {color[0], color[1], color[2]};
      meshJson["rotationSpeed"] = mesh->GetRotationSpeedDegPerSec();
      meshJson["albedoTexture"] = mesh->GetAlbedoTextureId();
      meshJson["meshId"] = mesh->GetMeshAssetId();
      components["MeshRenderer"] = std::move(meshJson);
    }

    if (auto light = entity->GetComponent<LightComponent>()) {
      Json lightJson;
      const auto color = light->GetColor();
      const auto ambient = light->GetAmbientColor();
      lightJson["lightEnabled"] = light->IsEnabled();
      lightJson["lightType"] = static_cast<int>(light->GetType());
      lightJson["lightColor"] = {color[0], color[1], color[2]};
      lightJson["lightIntensity"] = light->GetIntensity();
      lightJson["lightRange"] = light->GetRange();
      lightJson["innerConeAngle"] = light->GetInnerConeAngle();
      lightJson["outerConeAngle"] = light->GetOuterConeAngle();
      lightJson["lightPrimary"] = light->IsPrimary();
      lightJson["ambientColor"] = {ambient[0], ambient[1], ambient[2]};
      components["Light"] = std::move(lightJson);
    }

    if (auto camera = entity->GetComponent<CameraComponent>()) {
      Json cameraJson;
      cameraJson["projectionType"] =
          static_cast<int>(camera->GetProjectionType());
      cameraJson["verticalFov"] = camera->GetVerticalFov();
      cameraJson["nearClip"] = camera->GetNearClip();
      cameraJson["farClip"] = camera->GetFarClip();
      cameraJson["orthographicSize"] = camera->GetOrthographicSize();
      cameraJson["isPrimary"] = camera->IsPrimary();
      components["Camera"] = std::move(cameraJson);
    }

    if (auto rigidbody = entity->GetComponent<RigidbodyComponent>()) {
      Json rbJson;
      rbJson["motionType"] = static_cast<int>(rigidbody->GetMotionType());
      rbJson["mass"] = rigidbody->GetMass();
      rbJson["linearDamping"] = rigidbody->GetLinearDamping();
      rbJson["angularDamping"] = rigidbody->GetAngularDamping();
      rbJson["useGravity"] = rigidbody->UseGravity();
      rbJson["friction"] = rigidbody->GetFriction();
      rbJson["restitution"] = rigidbody->GetRestitution();
      components["Rigidbody"] = std::move(rbJson);
    }

    if (auto collider = entity->GetComponent<ColliderComponent>()) {
      Json colJson;
      colJson["shapeType"] = static_cast<int>(collider->GetShapeType());
      const auto halfExtents = collider->GetHalfExtents();
      colJson["halfExtents"] = {halfExtents[0], halfExtents[1], halfExtents[2]};
      colJson["radius"] = collider->GetRadius();
      colJson["height"] = collider->GetHeight();
      colJson["isTrigger"] = collider->IsTrigger();
      const auto offset = collider->GetOffset();
      colJson["offset"] = {offset[0], offset[1], offset[2]};
      components["Collider"] = std::move(colJson);
    }

    entityJson["components"] = std::move(components);
    root["entities"].push_back(std::move(entityJson));
  }

  out << root.dump(4);
  return true;
}

std::shared_ptr<Scene>
SceneSerializer::Load(const std::filesystem::path &path) const {
  std::ifstream input(path);
  if (!input.is_open()) {
    return nullptr;
  }

  Json root;
  try {
    input >> root;
  } catch (const nlohmann::json::exception &) {
    return nullptr;
  }

  auto scene = std::make_shared<Scene>(ReadString(root, "name", std::string()));
  scene->BindContext(m_context);

  auto entitiesIt = root.find("entities");
  if (entitiesIt == root.end() || !entitiesIt->is_array()) {
    return scene;
  }

  for (const auto &entityJson : *entitiesIt) {
    if (!entityJson.is_object()) {
      continue;
    }

    const Core::EntityId id = ReadEntityId(entityJson, "id", 0);
    const std::string name = ReadString(entityJson, "name", std::string());
    auto entity = std::make_shared<Entity>(id, name);

    auto componentsIt = entityJson.find("components");
    if (componentsIt != entityJson.end() && componentsIt->is_object()) {
      const Json &components = *componentsIt;

      auto transformIt = components.find("Transform");
      if (transformIt != components.end() && transformIt->is_object()) {
        const Json &transformJson = *transformIt;
        auto transform = std::make_shared<TransformComponent>();

        std::array<float, 3> position{0.0f, 0.0f, 0.0f};
        if (ReadVec3(transformJson, "position", position, 2)) {
          transform->SetPosition(position[0], position[1], position[2]);
        }

        std::array<float, 3> rotation{0.0f, 0.0f, 0.0f};
        const bool hasRotation =
            ReadVec3(transformJson, "rotation", rotation, 3);
        if (hasRotation) {
          transform->SetRotationDegrees(rotation[0], rotation[1], rotation[2]);
        } else {
          auto rotZIt = transformJson.find("rotationZ");
          if (rotZIt != transformJson.end() && rotZIt->is_number()) {
            rotation[2] = rotZIt->get<float>();
            transform->SetRotationDegrees(rotation[0], rotation[1],
                                          rotation[2]);
          }
        }

        std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
        if (ReadVec3(transformJson, "scale", scale, 2)) {
          transform->SetScale(scale[0], scale[1], scale[2]);
        }

        const Core::EntityId parentId =
            ReadEntityId(transformJson, "parent", 0);
        if (parentId != 0) {
          transform->SetParent(parentId);
        }

        entity->AddComponent(transform);
      }

      auto meshIt = components.find("MeshRenderer");
      if (meshIt != components.end() && meshIt->is_object()) {
        const Json &meshJson = *meshIt;
        auto mesh = std::make_shared<MeshRendererComponent>();

        mesh->SetVisible(ReadBool(meshJson, "visible", true));

        std::array<float, 3> meshColor{1.0f, 1.0f, 1.0f};
        if (ReadVec3(meshJson, "color", meshColor, 3)) {
          mesh->SetColor(meshColor[0], meshColor[1], meshColor[2]);
        }

        mesh->SetRotationSpeedDegPerSec(
            ReadNumber(meshJson, "rotationSpeed", 0.0f));

        const std::string meshId =
            ReadString(meshJson, "meshId", std::string());
        if (!meshId.empty()) {
          mesh->SetMeshAssetId(meshId);
        }

        const std::string albedoTexture =
            ReadString(meshJson, "albedoTexture", std::string());
        if (!albedoTexture.empty()) {
          mesh->SetAlbedoTextureId(albedoTexture);
        }

        entity->AddComponent(mesh);
      }

      auto lightIt = components.find("Light");
      if (lightIt != components.end() && lightIt->is_object()) {
        const Json &lightJson = *lightIt;
        auto light = std::make_shared<LightComponent>();

        light->SetEnabled(ReadBool(lightJson, "lightEnabled", true));

        int lightType = ReadInt(lightJson, "lightType", 0);
        if (lightType < 0 || lightType > 2) {
          lightType = 0;
        }
        light->SetType(static_cast<LightComponent::LightType>(lightType));

        std::array<float, 3> lightColor{1.0f, 1.0f, 1.0f};
        if (ReadVec3(lightJson, "lightColor", lightColor, 3)) {
          light->SetColor(lightColor[0], lightColor[1], lightColor[2]);
        }

        light->SetIntensity(ReadNumber(lightJson, "lightIntensity", 1.0f));
        light->SetRange(ReadNumber(lightJson, "lightRange", 10.0f));
        light->SetInnerConeAngle(
            ReadNumber(lightJson, "innerConeAngle", 15.0f));
        light->SetOuterConeAngle(
            ReadNumber(lightJson, "outerConeAngle", 30.0f));
        light->SetPrimary(ReadBool(lightJson, "lightPrimary", false));

        std::array<float, 3> ambientColor{0.0f, 0.0f, 0.0f};
        if (ReadVec3(lightJson, "ambientColor", ambientColor, 3)) {
          light->SetAmbientColor(ambientColor[0], ambientColor[1],
                                 ambientColor[2]);
        }

        entity->AddComponent(light);
      }

      auto cameraIt = components.find("Camera");
      if (cameraIt != components.end() && cameraIt->is_object()) {
        const Json &cameraJson = *cameraIt;
        auto camera = std::make_shared<CameraComponent>();

        int projectionType = ReadInt(cameraJson, "projectionType", 0);
        if (projectionType < 0 || projectionType > 1) {
          projectionType = 0;
        }
        camera->SetProjectionType(
            static_cast<CameraComponent::ProjectionType>(projectionType));
        camera->SetVerticalFov(
            ReadNumber(cameraJson, "verticalFov", camera->GetVerticalFov()));
        camera->SetNearClip(
            ReadNumber(cameraJson, "nearClip", camera->GetNearClip()));
        camera->SetFarClip(
            ReadNumber(cameraJson, "farClip", camera->GetFarClip()));
        camera->SetOrthographicSize(ReadNumber(cameraJson, "orthographicSize",
                                               camera->GetOrthographicSize()));
        camera->SetPrimary(ReadBool(cameraJson, "isPrimary", false));

        entity->AddComponent(camera);
      }

      auto rigidbodyIt = components.find("Rigidbody");
      if (rigidbodyIt != components.end() && rigidbodyIt->is_object()) {
        const Json &rbJson = *rigidbodyIt;
        auto rigidbody = std::make_shared<RigidbodyComponent>();

        int motionType = ReadInt(rbJson, "motionType", 2);
        if (motionType < 0 || motionType > 2)
          motionType = 2;
        rigidbody->SetMotionType(
            static_cast<RigidbodyComponent::MotionType>(motionType));
        rigidbody->SetMass(ReadNumber(rbJson, "mass", 1.0f));
        rigidbody->SetLinearDamping(ReadNumber(rbJson, "linearDamping", 0.05f));
        rigidbody->SetAngularDamping(
            ReadNumber(rbJson, "angularDamping", 0.05f));
        rigidbody->SetUseGravity(ReadBool(rbJson, "useGravity", true));
        rigidbody->SetFriction(ReadNumber(rbJson, "friction", 0.5f));
        rigidbody->SetRestitution(ReadNumber(rbJson, "restitution", 0.0f));

        entity->AddComponent(rigidbody);
      }

      auto colliderIt = components.find("Collider");
      if (colliderIt != components.end() && colliderIt->is_object()) {
        const Json &colJson = *colliderIt;
        auto collider = std::make_shared<ColliderComponent>();

        int shapeType = ReadInt(colJson, "shapeType", 0);
        if (shapeType < 0 || shapeType > 2)
          shapeType = 0;
        collider->SetShapeType(
            static_cast<ColliderComponent::ShapeType>(shapeType));

        std::array<float, 3> halfExtents{0.5f, 0.5f, 0.5f};
        if (ReadVec3(colJson, "halfExtents", halfExtents, 3)) {
          collider->SetHalfExtents(halfExtents[0], halfExtents[1],
                                   halfExtents[2]);
        }

        collider->SetRadius(ReadNumber(colJson, "radius", 0.5f));
        collider->SetHeight(ReadNumber(colJson, "height", 1.0f));
        collider->SetTrigger(ReadBool(colJson, "isTrigger", false));

        std::array<float, 3> offset{0.0f, 0.0f, 0.0f};
        if (ReadVec3(colJson, "offset", offset, 3)) {
          collider->SetOffset(offset[0], offset[1], offset[2]);
        }

        entity->AddComponent(collider);
      }
    }

    scene->AddEntity(entity);
  }

  // Rebuild child lists after all entities are present.
  for (const auto &entity : scene->GetEntities()) {
    if (!entity) {
      continue;
    }

    auto transform = entity->GetComponent<TransformComponent>();
    if (!transform || !transform->HasParent()) {
      continue;
    }

    auto parent = scene->FindEntityById(transform->GetParentId());
    if (!parent) {
      transform->ClearParent();
      continue;
    }

    auto parentTransform = parent->GetComponent<TransformComponent>();
    if (!parentTransform) {
      transform->ClearParent();
      continue;
    }

    parentTransform->AddChild(entity->GetId());
  }

  return scene;
}

std::shared_ptr<Scene> SceneSerializer::CreateDefaultScene() const {
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

  if (auto assets = m_context.GetAssetRegistry()) {
    auto root = assets->GetRootPath();
    if (root.empty()) {
      root = std::filesystem::current_path();
    }
    assets->Scan(root.string());
  }

  return scene;
}
} // namespace Aetherion::Scene
