#include "Aetherion/Physics/PhysicsWorld.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <string>
#include <thread>

// Jolt headers - must be included in specific order
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>


// Disable common warnings
JPH_SUPPRESS_WARNINGS

namespace {
void JoltTraceImpl(const char *fmt, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  Aetherion::Core::Log::Info(std::string("[Jolt] ") + buffer);
}

#ifdef JPH_ENABLE_ASSERTS
bool JoltAssertFailedImpl(const char *expression, const char *message,
                          const char *file, JPH::uint line) {
  std::string text = "Jolt assert failed: ";
  text += expression ? expression : "<unknown>";
  if (message) {
    text += " | ";
    text += message;
  }
  if (file) {
    text += " (";
    text += file;
    text += ":";
    text += std::to_string(line);
    text += ")";
  }
  Aetherion::Core::Log::Error(text);
  return false; // Don't break into the debugger by default.
}
#endif

void InstallJoltHandlersOnce() {
  static bool installed = false;
  if (installed) {
    return;
  }
  JPH::Trace = JoltTraceImpl;
  JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl;)
  installed = true;
}

// Layer definitions
namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
} // namespace Layers

namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr uint32_t NUM_LAYERS = 2;
} // namespace BroadPhaseLayers

/// Interface for broad phase layer mapping
class BroadPhaseLayerInterfaceImpl final
    : public JPH::BroadPhaseLayerInterface {
public:
  BroadPhaseLayerInterfaceImpl() {
    m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
  }

  virtual JPH::uint GetNumBroadPhaseLayers() const override {
    return BroadPhaseLayers::NUM_LAYERS;
  }

  virtual JPH::BroadPhaseLayer
  GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return m_objectToBroadPhase[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  virtual const char *
  GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
      return "NON_MOVING";
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
      return "MOVING";
    default:
      JPH_ASSERT(false);
      return "UNKNOWN";
    }
  }
#endif

private:
  JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
};

/// Filter to determine which objects can collide with which broad phase layers
class ObjectVsBroadPhaseLayerFilterImpl
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  virtual bool ShouldCollide(JPH::ObjectLayer inLayer1,
                             JPH::BroadPhaseLayer inLayer2) const override {
    switch (inLayer1) {
    case Layers::NON_MOVING:
      return inLayer2 == BroadPhaseLayers::MOVING;
    case Layers::MOVING:
      return true;
    default:
      JPH_ASSERT(false);
      return false;
    }
  }
};

/// Filter to determine which object layers can collide with each other
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
  virtual bool ShouldCollide(JPH::ObjectLayer inObject1,
                             JPH::ObjectLayer inObject2) const override {
    switch (inObject1) {
    case Layers::NON_MOVING:
      return inObject2 == Layers::MOVING;
    case Layers::MOVING:
      return true;
    default:
      JPH_ASSERT(false);
      return false;
    }
  }
};

// Helper to convert Euler degrees to quaternion
JPH::Quat EulerToQuaternion(float xDeg, float yDeg, float zDeg) {
  constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
  float x = xDeg * DEG_TO_RAD * 0.5f;
  float y = yDeg * DEG_TO_RAD * 0.5f;
  float z = zDeg * DEG_TO_RAD * 0.5f;

  float cx = std::cos(x), sx = std::sin(x);
  float cy = std::cos(y), sy = std::sin(y);
  float cz = std::cos(z), sz = std::sin(z);

  return JPH::Quat(sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz,
                   cx * cy * sz - sx * sy * cz, cx * cy * cz + sx * sy * sz);
}

} // anonymous namespace

namespace Aetherion::Physics {

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld() { Shutdown(); }

bool PhysicsWorld::Initialize() {
  if (m_initialized) {
    return true;
  }

  InstallJoltHandlersOnce();

  // Register Jolt allocators and types
  JPH::RegisterDefaultAllocator();

  // Create factory
  JPH::Factory::sInstance = new JPH::Factory();

  // Register physics types
  JPH::RegisterTypes();

  // Create temp allocator with fallback to avoid aborting on larger frames.
  constexpr JPH::uint tempAllocatorSize = 64 * 1024 * 1024;
  m_tempAllocator =
      std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(
          tempAllocatorSize);

  // Create job system with number of available threads
  const int numThreads =
      std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
  m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
      JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads);

  // Create broad phase layer interface
  m_broadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
  m_objectVsBroadPhaseLayerFilter =
      std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
  m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

  // Create physics system
  constexpr uint32_t maxBodies = 65536;
  constexpr uint32_t numBodyMutexes = 0; // default
  constexpr uint32_t maxBodyPairs = 65536;
  constexpr uint32_t maxContactConstraints = 65536;

  m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
  m_physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs,
                        maxContactConstraints, *m_broadPhaseLayerInterface,
                        *m_objectVsBroadPhaseLayerFilter,
                        *m_objectLayerPairFilter);

  // Set gravity
  m_physicsSystem->SetGravity(
      JPH::Vec3(m_gravity[0], m_gravity[1], m_gravity[2]));

  m_initialized = true;
  return true;
}

void PhysicsWorld::Shutdown() {
  if (!m_initialized) {
    return;
  }

  // Remove all bodies
  if (m_physicsSystem) {
    auto &bodyInterface = m_physicsSystem->GetBodyInterface();
    for (auto &entry : m_bodies) {
      if (entry.inUse && entry.bodyId) {
        bodyInterface.RemoveBody(*entry.bodyId);
        bodyInterface.DestroyBody(*entry.bodyId);
        delete entry.bodyId;
        entry.bodyId = nullptr;
        entry.inUse = false;
      }
    }
  }

  m_bodies.clear();
  m_freeIndices.clear();

  m_physicsSystem.reset();
  m_objectLayerPairFilter.reset();
  m_objectVsBroadPhaseLayerFilter.reset();
  m_broadPhaseLayerInterface.reset();
  m_jobSystem.reset();
  m_tempAllocator.reset();

  // Destroy factory
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;

  // Unregister types
  JPH::UnregisterTypes();

  m_initialized = false;
}

void PhysicsWorld::Step(float deltaTime) {
  if (!m_initialized || !m_physicsSystem || !m_tempAllocator || !m_jobSystem) {
    return;
  }

  if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
    return;
  }

  // Jolt recommends fixed timestep with multiple collision steps if needed     
  constexpr int collisionSteps = 1;
  const auto error =
      m_physicsSystem->Update(deltaTime, collisionSteps, m_tempAllocator.get(),
                              m_jobSystem.get());
  if (error != JPH::EPhysicsUpdateError::None) {
    Aetherion::Core::Log::Error("Jolt physics update failed.");
  }
}

BodyHandle
PhysicsWorld::CreateBody(const RigidbodyDesc &rigidbodyDesc,
                         const ColliderDesc &colliderDesc,
                         const std::array<float, 3> &position,
                         const std::array<float, 3> &rotationDegrees) {
  if (!m_initialized || !m_physicsSystem) {
    return BodyHandle{};
  }

  // Create shape based on collider type
  JPH::ShapeRefC shape;
  switch (colliderDesc.shapeType) {
  case ShapeType::Box:
    shape = new JPH::BoxShape(JPH::Vec3(colliderDesc.halfExtents[0],
                                        colliderDesc.halfExtents[1],
                                        colliderDesc.halfExtents[2]));
    break;
  case ShapeType::Sphere:
    shape = new JPH::SphereShape(colliderDesc.radius);
    break;
  case ShapeType::Capsule:
    shape =
        new JPH::CapsuleShape(colliderDesc.height * 0.5f, colliderDesc.radius);
    break;
  default:
    shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    break;
  }

  // Determine motion type
  JPH::EMotionType joltMotionType;
  JPH::ObjectLayer layer;
  switch (rigidbodyDesc.motionType) {
  case MotionType::Static:
    joltMotionType = JPH::EMotionType::Static;
    layer = Layers::NON_MOVING;
    break;
  case MotionType::Kinematic:
    joltMotionType = JPH::EMotionType::Kinematic;
    layer = Layers::MOVING;
    break;
  case MotionType::Dynamic:
  default:
    joltMotionType = JPH::EMotionType::Dynamic;
    layer = Layers::MOVING;
    break;
  }

  // Create body settings
  JPH::Quat rotation = EulerToQuaternion(rotationDegrees[0], rotationDegrees[1],
                                         rotationDegrees[2]);
  JPH::BodyCreationSettings bodySettings(
      shape, JPH::RVec3(position[0], position[1], position[2]), rotation,
      joltMotionType, layer);

  // Set mass properties for dynamic bodies
  if (rigidbodyDesc.motionType == MotionType::Dynamic) {
    bodySettings.mOverrideMassProperties =
        JPH::EOverrideMassProperties::CalculateInertia;
    bodySettings.mMassPropertiesOverride.mMass = rigidbodyDesc.mass;
  }

  bodySettings.mFriction = rigidbodyDesc.friction;
  bodySettings.mRestitution = rigidbodyDesc.restitution;
  bodySettings.mLinearDamping = rigidbodyDesc.linearDamping;
  bodySettings.mAngularDamping = rigidbodyDesc.angularDamping;
  bodySettings.mGravityFactor = rigidbodyDesc.useGravity ? 1.0f : 0.0f;

  // Create body
  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  JPH::BodyID bodyId =
      bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::Activate);

  if (bodyId.IsInvalid()) {
    return BodyHandle{};
  }

  // Find or create slot for body entry
  uint32_t index;
  if (!m_freeIndices.empty()) {
    index = m_freeIndices.back();
    m_freeIndices.pop_back();
  } else {
    index = static_cast<uint32_t>(m_bodies.size());
    m_bodies.push_back({});
  }

  auto &entry = m_bodies[index];
  entry.bodyId = new JPH::BodyID(bodyId);
  entry.entityId = rigidbodyDesc.entityId;
  entry.generation = m_nextGeneration++;
  entry.inUse = true;

  return BodyHandle{index, entry.generation};
}

void PhysicsWorld::DestroyBody(BodyHandle handle) {
  if (!m_initialized || !m_physicsSystem) {
    return;
  }

  if (handle.index >= m_bodies.size()) {
    return;
  }

  auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  bodyInterface.RemoveBody(*entry.bodyId);
  bodyInterface.DestroyBody(*entry.bodyId);

  delete entry.bodyId;
  entry.bodyId = nullptr;
  entry.inUse = false;
  entry.entityId = 0;

  m_freeIndices.push_back(handle.index);
}

JPH::Body *PhysicsWorld::GetJoltBody(BodyHandle handle) const {
  if (!m_initialized || !m_physicsSystem) {
    return nullptr;
  }

  if (handle.index >= m_bodies.size()) {
    return nullptr;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return nullptr;
  }

  return m_physicsSystem->GetBodyLockInterface().TryGetBody(*entry.bodyId);
}

BodyTransform PhysicsWorld::GetBodyTransform(BodyHandle handle) const {
  BodyTransform result;

  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {
    return result;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return result;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  JPH::RVec3 pos = bodyInterface.GetCenterOfMassPosition(*entry.bodyId);
  JPH::Quat rot = bodyInterface.GetRotation(*entry.bodyId);

  result.position = {static_cast<float>(pos.GetX()),
                     static_cast<float>(pos.GetY()),
                     static_cast<float>(pos.GetZ())};
  result.rotation = {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};

  return result;
}

void PhysicsWorld::SetBodyTransform(BodyHandle handle,
                                    const BodyTransform &transform) {
  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {
    return;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  bodyInterface.SetPositionAndRotation(
      *entry.bodyId,
      JPH::RVec3(transform.position[0], transform.position[1],
                 transform.position[2]),
      JPH::Quat(transform.rotation[0], transform.rotation[1],
                transform.rotation[2], transform.rotation[3]),
      JPH::EActivation::Activate);
}

void PhysicsWorld::ApplyForce(BodyHandle handle,
                              const std::array<float, 3> &force) {
  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {
    return;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  bodyInterface.AddForce(*entry.bodyId,
                         JPH::Vec3(force[0], force[1], force[2]));
}

void PhysicsWorld::ApplyImpulse(BodyHandle handle,
                                const std::array<float, 3> &impulse) {
  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {
    return;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  bodyInterface.AddImpulse(*entry.bodyId,
                           JPH::Vec3(impulse[0], impulse[1], impulse[2]));
}

void PhysicsWorld::SetLinearVelocity(BodyHandle handle,
                                     const std::array<float, 3> &velocity) {
  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {  
    return;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  bodyInterface.SetLinearVelocity(
      *entry.bodyId, JPH::Vec3(velocity[0], velocity[1], velocity[2]));
}

void PhysicsWorld::SetAngularVelocity(BodyHandle handle,
                                      const std::array<float, 3> &velocity) {
  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {
    return;
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return;
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  bodyInterface.SetAngularVelocity(
      *entry.bodyId, JPH::Vec3(velocity[0], velocity[1], velocity[2]));
}

std::array<float, 3> PhysicsWorld::GetLinearVelocity(BodyHandle handle) const {
  if (!m_initialized || !m_physicsSystem || handle.index >= m_bodies.size()) {
    return {0.0f, 0.0f, 0.0f};
  }

  const auto &entry = m_bodies[handle.index];
  if (!entry.inUse || entry.generation != handle.generation || !entry.bodyId) {
    return {0.0f, 0.0f, 0.0f};
  }

  auto &bodyInterface = m_physicsSystem->GetBodyInterface();
  JPH::Vec3 vel = bodyInterface.GetLinearVelocity(*entry.bodyId);
  return {vel.GetX(), vel.GetY(), vel.GetZ()};
}

void PhysicsWorld::SetGravity(const std::array<float, 3> &gravity) {
  m_gravity = gravity;
  if (m_initialized && m_physicsSystem) {
    m_physicsSystem->SetGravity(JPH::Vec3(gravity[0], gravity[1], gravity[2]));
  }
}

} // namespace Aetherion::Physics
