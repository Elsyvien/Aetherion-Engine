#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Aetherion/Core/Types.h"

// Forward declarations for Jolt types to avoid header pollution
namespace JPH
{
class PhysicsSystem;
class TempAllocatorImpl;
class JobSystemThreadPool;
class BroadPhaseLayerInterface;
class ObjectVsBroadPhaseLayerFilter;
class ObjectLayerPairFilter;
class BodyInterface;
class Body;
class BodyID;
} // namespace JPH

namespace Aetherion::Physics
{

/// @brief Motion type for rigid bodies
enum class MotionType : uint8_t
{
    Static = 0,    ///< Does not move, infinite mass
    Kinematic = 1, ///< Moves via user code, infinite mass
    Dynamic = 2    ///< Moves via physics simulation
};

/// @brief Shape type for colliders
enum class ShapeType : uint8_t
{
    Box = 0,
    Sphere = 1,
    Capsule = 2
};

/// @brief Descriptor for creating a rigid body
struct RigidbodyDesc
{
    Core::EntityId entityId{0};
    MotionType motionType{MotionType::Dynamic};
    float mass{1.0f};
    float linearDamping{0.05f};
    float angularDamping{0.05f};
    bool useGravity{true};
    float friction{0.5f};
    float restitution{0.0f};
};

/// @brief Descriptor for collider shape
struct ColliderDesc
{
    ShapeType shapeType{ShapeType::Box};
    std::array<float, 3> halfExtents{0.5f, 0.5f, 0.5f}; ///< For box
    float radius{0.5f};                                  ///< For sphere/capsule
    float height{1.0f};                                  ///< For capsule
    bool isTrigger{false};
};

/// @brief Handle to a physics body
struct BodyHandle
{
    uint32_t index{0};
    uint32_t generation{0};
    
    bool IsValid() const noexcept { return generation != 0; }
    bool operator==(const BodyHandle& other) const noexcept
    {
        return index == other.index && generation == other.generation;
    }
};

/// @brief Transform data for syncing between physics and scene
struct BodyTransform
{
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f}; // Quaternion (x,y,z,w)
};

/// @brief Main physics world wrapper around Jolt Physics
class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /// @brief Initialize the physics system
    /// @return true if successful
    bool Initialize();

    /// @brief Shutdown and release all resources
    void Shutdown();

    /// @brief Check if initialized
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /// @brief Step the physics simulation
    /// @param deltaTime Time step in seconds
    void Step(float deltaTime);

    /// @brief Create a rigid body with collider
    /// @param rigidbodyDesc Rigidbody properties
    /// @param colliderDesc Collider shape properties
    /// @param position Initial position
    /// @param rotation Initial rotation (Euler degrees)
    /// @return Handle to the created body
    BodyHandle CreateBody(const RigidbodyDesc& rigidbodyDesc,
                          const ColliderDesc& colliderDesc,
                          const std::array<float, 3>& position,
                          const std::array<float, 3>& rotationDegrees);

    /// @brief Destroy a rigid body
    /// @param handle Handle from CreateBody
    void DestroyBody(BodyHandle handle);

    /// @brief Get the current transform of a body
    /// @param handle Body handle
    /// @return Current position and rotation
    [[nodiscard]] BodyTransform GetBodyTransform(BodyHandle handle) const;

    /// @brief Set the transform of a body (for kinematic bodies or teleporting)
    /// @param handle Body handle
    /// @param transform New transform
    void SetBodyTransform(BodyHandle handle, const BodyTransform& transform);

    /// @brief Apply a force to a dynamic body
    void ApplyForce(BodyHandle handle, const std::array<float, 3>& force);

    /// @brief Apply an impulse to a dynamic body
    void ApplyImpulse(BodyHandle handle, const std::array<float, 3>& impulse);

    /// @brief Set linear velocity of a body
    void SetLinearVelocity(BodyHandle handle, const std::array<float, 3>& velocity);

    /// @brief Get linear velocity of a body
    [[nodiscard]] std::array<float, 3> GetLinearVelocity(BodyHandle handle) const;

    /// @brief Set gravity for the world
    void SetGravity(const std::array<float, 3>& gravity);

    /// @brief Get current gravity
    [[nodiscard]] std::array<float, 3> GetGravity() const noexcept { return m_gravity; }

private:
    struct BodyEntry
    {
        JPH::BodyID* bodyId{nullptr};
        Core::EntityId entityId{0};
        uint32_t generation{0};
        bool inUse{false};
    };

    [[nodiscard]] JPH::Body* GetJoltBody(BodyHandle handle) const;

    bool m_initialized{false};
    std::array<float, 3> m_gravity{0.0f, -9.81f, 0.0f};

    // Jolt Physics resources
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<JPH::BroadPhaseLayerInterface> m_broadPhaseLayerInterface;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_objectVsBroadPhaseLayerFilter;
    std::unique_ptr<JPH::ObjectLayerPairFilter> m_objectLayerPairFilter;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

    // Body management
    std::vector<BodyEntry> m_bodies;
    std::vector<uint32_t> m_freeIndices;
    uint32_t m_nextGeneration{1};
};

} // namespace Aetherion::Physics
