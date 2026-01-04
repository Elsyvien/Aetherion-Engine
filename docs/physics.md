# Physics

## PhysicsWorld (Jolt Wrapper)
Files:
- `Engine/Physics/include/Aetherion/Physics/PhysicsWorld.h`
- `Engine/Physics/src/PhysicsWorld.cpp`

PhysicsWorld wraps Jolt Physics and manages bodies:
- Initialize(): registers Jolt types and creates PhysicsSystem.
- Shutdown(): destroys bodies and Jolt factory.
- Step(deltaTime): advances the simulation.

Body creation:
- CreateBody(RigidbodyDesc, ColliderDesc, position, rotationDegrees)
- Supports Box, Sphere, Capsule shapes.
- Motion type: Static, Kinematic, Dynamic.
- BodyHandle contains index + generation for validation.

Transform sync:
- GetBodyTransform returns position + quaternion.
- SetBodyTransform teleports/kinematically moves bodies.

Forces:
- ApplyForce, ApplyImpulse, SetLinearVelocity, SetAngularVelocity.

## PhysicsContactListener
Files:
- `Engine/Physics/include/Aetherion/Physics/PhysicsContactListener.h`
- `Engine/Physics/src/PhysicsContactListener.cpp`

- Receives Jolt contact events and queues them for main thread processing.
- Maps BodyID to EntityId via a callback.
- Emits Enter/Stay events; Exit is currently not queued.

## PhysicsSystem (Scene Sync)
Files:
- `Engine/Physics/include/Aetherion/Physics/PhysicsSystem.h`
- `Engine/Physics/src/PhysicsSystem.cpp`

Responsibilities:
- Bind a Scene and create bodies for entities that have:
  TransformComponent + RigidbodyComponent + ColliderComponent.
- Recreate bodies when components are marked dirty.
- Fixed timestep stepping and accumulator.
- Write back dynamic body transforms to TransformComponents.
