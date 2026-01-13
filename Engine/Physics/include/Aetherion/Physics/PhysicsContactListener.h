#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <array>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Physics {

/// @brief Collision event data passed to callbacks
struct CollisionEvent {
  Core::EntityId entityA{0};
  Core::EntityId entityB{0};
  std::array<float, 3> contactPoint{0.0f, 0.0f, 0.0f};
  std::array<float, 3> contactNormal{0.0f, 0.0f, 0.0f};
  float penetrationDepth{0.0f};
  float impulse{0.0f};
  bool isSensorA{false};
  bool isSensorB{false};
};

/// @brief Types of collision events
enum class CollisionEventType : uint8_t {
  Enter = 0, ///< First frame of collision
  Stay = 1,  ///< Continuing collision
  Exit = 2   ///< Last frame of collision
};

/// @brief Callback signature for collision events
using CollisionCallback =
    std::function<void(CollisionEventType, const CollisionEvent &)>;

/// @brief Internal contact listener that receives Jolt physics events
/// and queues them for processing on the main thread
class PhysicsContactListener final : public JPH::ContactListener {
public:
  PhysicsContactListener();
  ~PhysicsContactListener() override = default;

  // Jolt interface
  JPH::ValidateResult
  OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2,
                    JPH::RVec3Arg inBaseOffset,
                    const JPH::CollideShapeResult &inCollisionResult) override;

  void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2,
                      const JPH::ContactManifold &inManifold,
                      JPH::ContactSettings &ioSettings) override;

  void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2,
                          const JPH::ContactManifold &inManifold,
                          JPH::ContactSettings &ioSettings) override;

  void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override;

  // Engine interface

  /// @brief Set the global collision callback
  void SetCallback(CollisionCallback callback) {
    m_callback = std::move(callback);
  }

  /// @brief Process queued collision events (call from main thread)
  void ProcessEvents();

  /// @brief Clear all queued events
  void ClearEvents();

  /// @brief Set the mapping function from BodyID to EntityId
  void SetBodyToEntityMapper(std::function<Core::EntityId(uint32_t)> mapper) {
    m_bodyToEntity = std::move(mapper);
  }

  /// @brief Register a body ID for contact tracking (call when body is created)
  void RegisterBody(uint32_t bodyId, Core::EntityId entityId, bool isSensor);

  /// @brief Unregister a body ID (call when body is destroyed)
  void UnregisterBody(uint32_t bodyId);

private:
  struct QueuedEvent {
    CollisionEventType type;
    CollisionEvent event;
  };

  /// @brief Key for tracking active contact pairs
  struct ContactPairKey {
    uint64_t subShapeIdPairValue; // Combined SubShapeIDPair value for lookup

    bool operator==(const ContactPairKey &other) const {
      return subShapeIdPairValue == other.subShapeIdPairValue;
    }
  };

  struct ContactPairKeyHash {
    std::size_t operator()(const ContactPairKey &key) const {
      return std::hash<uint64_t>{}(key.subShapeIdPairValue);
    }
  };

  /// @brief Cached entity IDs for a contact pair (needed for exit events)
  struct ContactPairInfo {
    Core::EntityId entityA{0};
    Core::EntityId entityB{0};
    bool isSensorA{false};
    bool isSensorB{false};
  };

  std::vector<QueuedEvent> m_eventQueue;
  std::mutex m_queueMutex;
  CollisionCallback m_callback;
  std::function<Core::EntityId(uint32_t)> m_bodyToEntity;

  /// @brief Maps SubShapeIDPair to cached entity IDs for exit event handling
  std::unordered_map<ContactPairKey, ContactPairInfo, ContactPairKeyHash>
      m_activeContacts;
  std::mutex m_contactsMutex;

  struct BodyCacheInfo {
    Core::EntityId entityId{0};
    bool isSensor{false};
  };

  /// @brief Maps BodyID to cached info for exit events (since bodies aren't available)
  std::unordered_map<uint32_t, BodyCacheInfo> m_bodyToEntityCache;
  std::mutex m_bodyCacheMutex;

  /// @brief Create a ContactPairKey from SubShapeIDPair
  static ContactPairKey MakeContactKey(const JPH::SubShapeIDPair &pair);
};

} // namespace Aetherion::Physics
