#include "Aetherion/Physics/PhysicsContactListener.h"

#include <Jolt/Physics/Collision/CollideShape.h>

namespace Aetherion::Physics {

PhysicsContactListener::PhysicsContactListener() { m_eventQueue.reserve(64); }

JPH::ValidateResult PhysicsContactListener::OnContactValidate(
    [[maybe_unused]] const JPH::Body &inBody1,
    [[maybe_unused]] const JPH::Body &inBody2,
    [[maybe_unused]] JPH::RVec3Arg inBaseOffset,
    [[maybe_unused]] const JPH::CollideShapeResult &inCollisionResult) {
  // Accept all contacts by default
  return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

PhysicsContactListener::ContactPairKey
PhysicsContactListener::MakeContactKey(const JPH::SubShapeIDPair &pair) {
  // Combine the two body IDs and sub-shape IDs into a single 64-bit key
  // This is a simplified approach - we use the hash of the pair
  uint64_t body1 = pair.GetBody1ID().GetIndexAndSequenceNumber();
  uint64_t body2 = pair.GetBody2ID().GetIndexAndSequenceNumber();
  // Ensure consistent ordering for the same pair regardless of order
  if (body1 > body2) {
    std::swap(body1, body2);
  }
  return ContactPairKey{(body1 << 32) | body2};
}

void PhysicsContactListener::RegisterBody(uint32_t bodyId,
                                          Core::EntityId entityId,
                                          bool isSensor) {
  std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
  m_bodyToEntityCache[bodyId] = {entityId, isSensor};
}

void PhysicsContactListener::UnregisterBody(uint32_t bodyId) {
  std::vector<QueuedEvent> exitEvents;
  {
    std::lock_guard<std::mutex> lock(m_contactsMutex);
    for (auto it = m_activeContacts.begin(); it != m_activeContacts.end();) {
      const uint64_t keyValue = it->first.subShapeIdPairValue;
      const uint32_t bodyA = static_cast<uint32_t>(keyValue >> 32);
      const uint32_t bodyB = static_cast<uint32_t>(keyValue & 0xffffffffu);
      if (bodyA == bodyId || bodyB == bodyId) {
        CollisionEvent event;
        event.entityA = it->second.entityA;
        event.entityB = it->second.entityB;
        event.isSensorA = it->second.isSensorA;
        event.isSensorB = it->second.isSensorB;
        exitEvents.push_back({CollisionEventType::Exit, event});
        it = m_activeContacts.erase(it);
      } else {
        ++it;
      }
    }
  }

  if (!exitEvents.empty()) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.insert(m_eventQueue.end(), exitEvents.begin(),
                        exitEvents.end());
  }

  std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
  m_bodyToEntityCache.erase(bodyId);
}

void PhysicsContactListener::OnContactAdded(
    const JPH::Body &inBody1, const JPH::Body &inBody2,
    const JPH::ContactManifold &inManifold,
    [[maybe_unused]] JPH::ContactSettings &ioSettings) {
  auto resolveEntity = [this](uint32_t bodyId) -> Core::EntityId {
    if (m_bodyToEntity) {
      return m_bodyToEntity(bodyId);
    }
    std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
    auto it = m_bodyToEntityCache.find(bodyId);
    return it != m_bodyToEntityCache.end() ? it->second.entityId
                                           : Core::EntityId{};
  };

  CollisionEvent event;
  event.entityA = resolveEntity(inBody1.GetID().GetIndexAndSequenceNumber());
  event.entityB = resolveEntity(inBody2.GetID().GetIndexAndSequenceNumber());
  if (event.entityA == 0 && event.entityB == 0) {
    return;
  }

  // Get first contact point if available
  if (!inManifold.mRelativeContactPointsOn1.empty()) {
    auto contactPos = inManifold.GetWorldSpaceContactPointOn1(0);
    event.contactPoint = {static_cast<float>(contactPos.GetX()),
                          static_cast<float>(contactPos.GetY()),
                          static_cast<float>(contactPos.GetZ())};
  }

  event.contactNormal = {inManifold.mWorldSpaceNormal.GetX(),
                         inManifold.mWorldSpaceNormal.GetY(),
                         inManifold.mWorldSpaceNormal.GetZ()};
  event.penetrationDepth = inManifold.mPenetrationDepth;
  event.impulse = 0.0f; // Not available yet during OnContactAdded
  event.isSensorA = inBody1.IsSensor();
  event.isSensorB = inBody2.IsSensor();

  // Cache the contact pair for exit event handling
  {
    JPH::SubShapeIDPair pair(inBody1.GetID(), inManifold.mSubShapeID1,
                             inBody2.GetID(), inManifold.mSubShapeID2);
    ContactPairKey key = MakeContactKey(pair);
    ContactPairInfo info{event.entityA, event.entityB, event.isSensorA,
                         event.isSensorB};
    std::lock_guard<std::mutex> lock(m_contactsMutex);
    m_activeContacts[key] = info;
  }

  std::lock_guard<std::mutex> lock(m_queueMutex);
  m_eventQueue.push_back({CollisionEventType::Enter, event});
}

void PhysicsContactListener::OnContactPersisted(
    const JPH::Body &inBody1, const JPH::Body &inBody2,
    const JPH::ContactManifold &inManifold,
    [[maybe_unused]] JPH::ContactSettings &ioSettings) {
  auto resolveEntity = [this](uint32_t bodyId) -> Core::EntityId {
    if (m_bodyToEntity) {
      return m_bodyToEntity(bodyId);
    }
    std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
    auto it = m_bodyToEntityCache.find(bodyId);
    return it != m_bodyToEntityCache.end() ? it->second.entityId
                                           : Core::EntityId{};
  };

  CollisionEvent event;
  event.entityA = resolveEntity(inBody1.GetID().GetIndexAndSequenceNumber());
  event.entityB = resolveEntity(inBody2.GetID().GetIndexAndSequenceNumber());
  if (event.entityA == 0 && event.entityB == 0) {
    return;
  }

  // Get first contact point if available
  if (!inManifold.mRelativeContactPointsOn1.empty()) {
    auto contactPos = inManifold.GetWorldSpaceContactPointOn1(0);
    event.contactPoint = {static_cast<float>(contactPos.GetX()),
                          static_cast<float>(contactPos.GetY()),
                          static_cast<float>(contactPos.GetZ())};
  }

  event.contactNormal = {inManifold.mWorldSpaceNormal.GetX(),
                         inManifold.mWorldSpaceNormal.GetY(),
                         inManifold.mWorldSpaceNormal.GetZ()};
  event.penetrationDepth = inManifold.mPenetrationDepth;
  event.impulse = 0.0f;
  event.isSensorA = inBody1.IsSensor();
  event.isSensorB = inBody2.IsSensor();

  std::lock_guard<std::mutex> lock(m_queueMutex);
  m_eventQueue.push_back({CollisionEventType::Stay, event});
}

void PhysicsContactListener::OnContactRemoved(
    const JPH::SubShapeIDPair &inSubShapePair) {
  // Look up the cached entity IDs for this contact pair
  ContactPairKey key = MakeContactKey(inSubShapePair);
  ContactPairInfo info;
  bool found = false;

  {
    std::lock_guard<std::mutex> lock(m_contactsMutex);
    auto it = m_activeContacts.find(key);
    if (it != m_activeContacts.end()) {
      info = it->second;
      found = true;
      m_activeContacts.erase(it);
    }
  }

  if (!found) {
    // Fallback: try to get entity IDs from the body cache
    uint32_t body1Id = inSubShapePair.GetBody1ID().GetIndexAndSequenceNumber();
    uint32_t body2Id = inSubShapePair.GetBody2ID().GetIndexAndSequenceNumber();

    std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
    auto it1 = m_bodyToEntityCache.find(body1Id);
    auto it2 = m_bodyToEntityCache.find(body2Id);
    if (it1 != m_bodyToEntityCache.end() &&
        it2 != m_bodyToEntityCache.end()) {
      info.entityA = it1->second.entityId;
      info.entityB = it2->second.entityId;
      info.isSensorA = it1->second.isSensor;
      info.isSensorB = it2->second.isSensor;
      found = true;
    }
  }

  if (found) {
    CollisionEvent event;
    event.entityA = info.entityA;
    event.entityB = info.entityB;
    // Contact point/normal not available during OnContactRemoved
    event.contactPoint = {0.0f, 0.0f, 0.0f};
    event.contactNormal = {0.0f, 0.0f, 0.0f};
    event.penetrationDepth = 0.0f;
    event.impulse = 0.0f;
    event.isSensorA = info.isSensorA;
    event.isSensorB = info.isSensorB;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push_back({CollisionEventType::Exit, event});
  }
}

void PhysicsContactListener::ProcessEvents() {
  std::vector<QueuedEvent> eventsToProcess;
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    eventsToProcess.swap(m_eventQueue);
  }

  if (!m_callback) {
    return;
  }

  for (const auto &queued : eventsToProcess) {
    m_callback(queued.type, queued.event);
  }
}

void PhysicsContactListener::ClearEvents() {
  std::lock_guard<std::mutex> lock(m_queueMutex);
  m_eventQueue.clear();
}

} // namespace Aetherion::Physics
