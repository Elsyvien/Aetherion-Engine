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
    [[maybe_unused]] const JPH::SubShapeIDPair &inSubShapePair) {
  // Note: We cannot access bodies during OnContactRemoved
  // For now, we don't queue exit events since we don't have entity mapping
  // A more sophisticated implementation would cache body-to-entity mappings
  // from OnContactAdded/Persisted and use them here
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
