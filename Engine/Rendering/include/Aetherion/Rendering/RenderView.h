#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene {
class TransformComponent;
class MeshRendererComponent;
} // namespace Aetherion::Scene

namespace Aetherion::Rendering {
enum class RenderLightType : uint32_t { Directional = 0, Point = 1, Spot = 2 };

struct RenderInstance {
  Core::EntityId entityId{0};
  const Scene::TransformComponent *transform{nullptr};
  const Scene::MeshRendererComponent *mesh{nullptr};
  std::string meshAssetId;
  std::string albedoTextureId;
  float model[16]{};
  bool hasModel{false};
};

struct RenderBatch {
  std::vector<RenderInstance> instances;
};

struct RenderDirectionalLight {
  bool enabled{false};
  float direction[3]{0.0f, -1.0f, 0.0f};
  float color[3]{1.0f, 1.0f, 1.0f};
  float intensity{1.0f};
  float ambientColor[3]{0.18f, 0.18f, 0.20f};
  // For gizmo rendering
  float position[3]{0.0f, 3.0f, 0.0f};
  Core::EntityId entityId{0};
};

struct RenderLight {
  RenderLightType type{RenderLightType::Directional};
  bool enabled{true};
  float position[3]{0.0f, 0.0f, 0.0f};
  float direction[3]{0.0f, -1.0f, 0.0f};
  float color[3]{1.0f, 1.0f, 1.0f};
  float intensity{1.0f};
  float range{10.0f};
  float innerConeAngle{15.0f};
  float outerConeAngle{30.0f};
  bool isPrimary{false};
  Core::EntityId entityId{0};
};

struct RenderCamera {
  bool enabled{false};
  float position[3]{0.0f, 0.0f, 0.0f};
  float forward[3]{0.0f, 0.0f, -1.0f};
  float up[3]{0.0f, 1.0f, 0.0f};
  float verticalFov{60.0f};
  float nearClip{0.1f};
  float farClip{100.0f};
  float orthographicSize{10.0f};
  uint32_t projectionType{0};
  Core::EntityId entityId{0};
};

struct RenderCollider {
  Core::EntityId entityId{0};
  uint32_t shapeType{0}; // 0=Box, 1=Sphere, 2=Capsule
  float halfExtents[3]{0.5f, 0.5f, 0.5f};
  float radius{0.5f};
  float height{1.0f};
  float offset[3]{0.0f, 0.0f, 0.0f};
  float worldMatrix[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool isTrigger{false};
  bool isStatic{false};
};

struct RenderView {
  std::vector<RenderInstance> instances;
  std::vector<RenderBatch> batches;
  std::unordered_map<Core::EntityId, const Scene::TransformComponent *>
      transforms;
  std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent *>
      meshes;
  Core::EntityId selectedEntityId{0};
  RenderDirectionalLight directionalLight{};
  std::vector<RenderLight> lights;
  RenderCamera camera{};
  std::vector<RenderCamera> cameras;
  std::vector<RenderCollider> colliders;
  bool showEditorIcons{false};
  bool showColliders{true};
};
} // namespace Aetherion::Rendering
