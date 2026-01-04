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

### Missing/Planned
- Queue Exit events to match Enter/Stay lifecycle.
- Editor debug visualization for colliders and contact points.

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

---

## Known Gaps & Planned Improvements

### Missing: Contact Exit Events

**Problem**: `PhysicsContactListener` currently emits Enter and Stay events but
does not queue Exit events when contacts end.

**Impact**: Scripts and systems cannot detect when a collision ends (e.g.,
leaving a trigger zone).

**Fix Plan**:
1. Track active contact pairs in `PhysicsContactListener`.
2. On Jolt `OnContactRemoved` callback, emit Exit event.
3. Queue exit events for main thread dispatch like enter/stay.
4. Expose `on_collision_exit` in scripting bindings.

**Checklist**:
- [ ] Add contact pair tracking set.
- [ ] Implement `OnContactRemoved` handler.
- [ ] Queue and dispatch exit events.
- [ ] Add unit test for enter → stay → exit sequence.

---

### Missing: Editor Debug Visualization

**Problem**: The editor does not visualize collider shapes, making it hard to
debug physics setups.

**Planned Features**:
- [ ] Wireframe overlay for Box, Sphere, Capsule colliders.
- [ ] Color coding: static (gray), kinematic (blue), dynamic (green).
- [ ] Highlight sleeping bodies differently.
- [ ] Show contact points as small spheres during simulation.
- [ ] Show velocity vectors as arrows.
- [ ] Toggle visibility per collider type.

**Implementation Notes**:
- Collider geometry is already in `RenderView::Colliders` for debug draw.
- Need a debug line/shape renderer pass in `VulkanViewport`.
- Editor toolbar toggle: "Show Physics Debug".

---

### Missing: Editor Tooling Polish

**Planned Improvements**:
- [ ] Inspector: editable collider dimensions with live preview.
- [ ] Inspector: rigidbody mass, friction, restitution sliders.
- [ ] Gizmo: resize collider by dragging handles.
- [ ] Scene view: visualize center of mass.
- [ ] Play mode: real-time physics stats overlay (body count, contacts, step time).
