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
                                          Core::EntityId entityId) {
  std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
  m_bodyToEntityCache[bodyId] = entityId;
}

void PhysicsContactListener::UnregisterBody(uint32_t bodyId) {
  std::lock_guard<std::mutex> lock(m_bodyCacheMutex);
  m_bodyToEntityCache.erase(bodyId);
}

void PhysicsContactListener::OnContactAdded(
    const JPH::Body &inBody1, const JPH::Body &inBody2,
    const JPH::ContactManifold &inManifold,
    [[maybe_unused]] JPH::ContactSettings &ioSettings) {
  if (!m_bodyToEntity) {
    return;
  }

  CollisionEvent event;
  event.entityA = m_bodyToEntity(inBody1.GetID().GetIndexAndSequenceNumber());
  event.entityB = m_bodyToEntity(inBody2.GetID().GetIndexAndSequenceNumber());

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

  // Cache the contact pair for exit event handling
  {
    JPH::SubShapeIDPair pair(inBody1.GetID(), inManifold.mSubShapeID1,
                             inBody2.GetID(), inManifold.mSubShapeID2);
    ContactPairKey key = MakeContactKey(pair);
    ContactPairInfo info{event.entityA, event.entityB};
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
  if (!m_bodyToEntity) {
    return;
  }

  CollisionEvent event;
  event.entityA = m_bodyToEntity(inBody1.GetID().GetIndexAndSequenceNumber());
  event.entityB = m_bodyToEntity(inBody2.GetID().GetIndexAndSequenceNumber());

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
      info.entityA = it1->second;
      info.entityB = it2->second;
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
