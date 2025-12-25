#include "Aetherion/Physics/PhysicsSystem.h"

#include <cmath>
#include <unordered_set>

#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace {

// Convert quaternion to Euler angles (in degrees)
std::array<float, 3> QuaternionToEuler(const std::array<float, 4> &q) {
  constexpr float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;

  float x = q[0], y = q[1], z = q[2], w = q[3];

  // Roll (x-axis rotation)
  float sinr_cosp = 2.0f * (w * x + y * z);
  float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
  float roll = std::atan2(sinr_cosp, cosr_cosp);

  // Pitch (y-axis rotation)
  float sinp = 2.0f * (w * y - z * x);
  float pitch;
  if (std::abs(sinp) >= 1.0f) {
    pitch = std::copysign(3.14159265358979323846f / 2.0f, sinp);
  } else {
    pitch = std::asin(sinp);
  }

  // Yaw (z-axis rotation)
  float siny_cosp = 2.0f * (w * z + x * y);
  float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
  float yaw = std::atan2(siny_cosp, cosy_cosp);

  return {roll * RAD_TO_DEG, pitch * RAD_TO_DEG, yaw * RAD_TO_DEG};
}

} // anonymous namespace

namespace Aetherion::Physics {

PhysicsSystem::PhysicsSystem(std::shared_ptr<PhysicsWorld> physicsWorld)
    : m_physicsWorld(std::move(physicsWorld))
{
  if (!m_physicsWorld)
  {
    m_physicsWorld = std::make_shared<PhysicsWorld>();
    m_ownsPhysicsWorld = true;
  }
}

PhysicsSystem::~PhysicsSystem() { Shutdown(); }

bool PhysicsSystem::Initialize() {
  return m_physicsWorld && m_physicsWorld->Initialize();
}

void PhysicsSystem::Shutdown() {
  UnbindScene();
  if (m_physicsWorld && m_ownsPhysicsWorld) {
    m_physicsWorld->Shutdown();
  }
}

void PhysicsSystem::BindScene(Scene::Scene *scene) {
  if (m_scene == scene) {
    return;
  }

  UnbindScene();
  m_scene = scene;

  if (m_scene) {
    SyncBodies();
  }
}

void PhysicsSystem::UnbindScene() {
  // Destroy all physics bodies
  for (const auto &[entityId, handle] : m_entityBodies) {
    if (m_physicsWorld) {
      m_physicsWorld->DestroyBody(handle);
    }
  }
  m_entityBodies.clear();
  m_scene = nullptr;
  m_accumulator = 0.0f;
}

void PhysicsSystem::SyncBodies() {
  if (!m_scene || !m_physicsWorld) {
    return;
  }

  const auto &entities = m_scene->GetEntities();

  // Track which entities still exist
  std::unordered_set<Core::EntityId> existingEntities;

  for (const auto &entity : entities) {
    if (!entity) {
      continue;
    }

    Core::EntityId entityId = entity->GetId();
    auto rigidbody = entity->GetComponent<Scene::RigidbodyComponent>();
    auto collider = entity->GetComponent<Scene::ColliderComponent>();
    auto transform = entity->GetComponent<Scene::TransformComponent>();

    // Need all three components for physics
    if (!rigidbody || !collider || !transform) {
      // Remove body if it exists but components are gone
      auto it = m_entityBodies.find(entityId);
      if (it != m_entityBodies.end()) {
        m_physicsWorld->DestroyBody(it->second);
        m_entityBodies.erase(it);
      }
      continue;
    }

    existingEntities.insert(entityId);

    auto it = m_entityBodies.find(entityId);
    bool needsRecreate = (it == m_entityBodies.end()) || rigidbody->IsDirty() ||
                         collider->IsDirty();

    if (needsRecreate) {
      // Destroy existing body if any
      if (it != m_entityBodies.end()) {
        m_physicsWorld->DestroyBody(it->second);
        m_entityBodies.erase(it);
      }

      CreateBodyForEntity(entityId);
      rigidbody->ClearDirty();
      collider->ClearDirty();
    }
  }

  // Remove bodies for entities that no longer exist
  for (auto it = m_entityBodies.begin(); it != m_entityBodies.end();) {
    if (existingEntities.find(it->first) == existingEntities.end()) {
      m_physicsWorld->DestroyBody(it->second);
      it = m_entityBodies.erase(it);
    } else {
      ++it;
    }
  }
}

void PhysicsSystem::CreateBodyForEntity(Core::EntityId entityId) {
  if (!m_scene || !m_physicsWorld) {
    return;
  }

  auto entity = m_scene->FindEntityById(entityId);
  if (!entity) {
    return;
  }

  auto rigidbody = entity->GetComponent<Scene::RigidbodyComponent>();
  auto collider = entity->GetComponent<Scene::ColliderComponent>();
  auto transform = entity->GetComponent<Scene::TransformComponent>();

  if (!rigidbody || !collider || !transform) {
    return;
  }

  RigidbodyDesc rbDesc;
  rbDesc.entityId = entityId;
  rbDesc.motionType = rigidbody->GetMotionType();
  rbDesc.mass = rigidbody->GetMass();
  rbDesc.linearDamping = rigidbody->GetLinearDamping();
  rbDesc.angularDamping = rigidbody->GetAngularDamping();
  rbDesc.useGravity = rigidbody->UseGravity();
  rbDesc.friction = rigidbody->GetFriction();
  rbDesc.restitution = rigidbody->GetRestitution();

  ColliderDesc colDesc;
  colDesc.shapeType = collider->GetShapeType();
  colDesc.halfExtents = collider->GetHalfExtents();
  colDesc.radius = collider->GetRadius();
  colDesc.height = collider->GetHeight();
  colDesc.isTrigger = collider->IsTrigger();

  std::array<float, 3> position = {transform->GetPositionX(),
                                   transform->GetPositionY(),
                                   transform->GetPositionZ()};

  std::array<float, 3> rotation = {transform->GetRotationXDegrees(),
                                   transform->GetRotationYDegrees(),
                                   transform->GetRotationZDegrees()};

  BodyHandle handle =
      m_physicsWorld->CreateBody(rbDesc, colDesc, position, rotation);
  if (handle.IsValid()) {
    m_entityBodies[entityId] = handle;
    rigidbody->SetBodyHandle(handle);
  }
}

void PhysicsSystem::DestroyBodyForEntity(Core::EntityId entityId) {
  auto it = m_entityBodies.find(entityId);
  if (it == m_entityBodies.end()) {
    return;
  }

  if (m_physicsWorld) {
    m_physicsWorld->DestroyBody(it->second);
  }

  // Clear handle in rigidbody component if entity still exists
  if (m_scene) {
    auto entity = m_scene->FindEntityById(entityId);
    if (entity) {
      auto rigidbody = entity->GetComponent<Scene::RigidbodyComponent>();
      if (rigidbody) {
        rigidbody->SetBodyHandle({});
      }
    }
  }

  m_entityBodies.erase(it);
}

void PhysicsSystem::Update(float deltaTime) {
  if (!m_enabled || !m_scene || !m_physicsWorld ||
      !m_physicsWorld->IsInitialized()) {
    return;
  }

  // Sync any new/changed bodies
  SyncBodies();

  // Fixed timestep accumulator
  m_accumulator += deltaTime;

  while (m_accumulator >= m_fixedTimestep) {
    m_physicsWorld->Step(m_fixedTimestep);
    m_accumulator -= m_fixedTimestep;
  }

  // Write physics transforms back to scene
  WriteBackTransforms();
}

void PhysicsSystem::WriteBackTransforms() {
  if (!m_scene || !m_physicsWorld) {
    return;
  }

  for (const auto &[entityId, handle] : m_entityBodies) {
    auto entity = m_scene->FindEntityById(entityId);
    if (!entity) {
      continue;
    }

    auto rigidbody = entity->GetComponent<Scene::RigidbodyComponent>();
    auto transform = entity->GetComponent<Scene::TransformComponent>();

    if (!rigidbody || !transform) {
      continue;
    }

    // Only update dynamic bodies
    if (rigidbody->GetMotionType() != MotionType::Dynamic) {
      continue;
    }

    BodyTransform bodyTransform = m_physicsWorld->GetBodyTransform(handle);

    transform->SetPosition(bodyTransform.position[0], bodyTransform.position[1],
                           bodyTransform.position[2]);

    // Convert quaternion to Euler angles
    auto euler = QuaternionToEuler(bodyTransform.rotation);
    transform->SetRotationDegrees(euler[0], euler[1], euler[2]);
  }
}

} // namespace Aetherion::Physics
