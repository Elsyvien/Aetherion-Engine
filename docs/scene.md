# Scene System

## Core Types
Files:
- `Engine/Scene/include/Aetherion/Scene/Component.h`
- `Engine/Scene/include/Aetherion/Scene/Entity.h`
- `Engine/Scene/include/Aetherion/Scene/Scene.h`
- `Engine/Scene/include/Aetherion/Scene/System.h`

### Component
- Base class with lifecycle hooks: OnAdded, OnRemoved, OnBeginPlay, OnEndPlay,
  OnUpdate(deltaTime).
- `Bind`/`Unbind` is controlled by Entity/Scene, not public.
- `Update` only runs after BeginPlay.

### Entity
- Owns an ID, name, and a vector of Components.
- `AddComponent` binds to scene and triggers BeginPlay if the scene is playing.
- `RemoveComponent` unbinds and removes.
- `GetComponent<T>()` and `HasComponent<T>()` are dynamic cast helpers.

### Scene
- Owns entities, systems, and play state.
- `CreateEntity()` allocates IDs (monotonic).
- `SetParent(childId, newParentId)` updates Transform hierarchy and prevents
  cycles by checking ancestors.
- `Tick(deltaTime, playing, paused, stepRequested)` handles BeginPlay/EndPlay
  and updates components while playing.

### System
- Abstract interface for scene-level systems:
  - `Configure(EngineContext&)`
  - `Update(Scene&, deltaTime)`

## Transform Hierarchy
File: `Engine/Scene/include/Aetherion/Scene/TransformComponent.h`

TransformComponent stores:
- position (x,y,z)
- rotation degrees (x,y,z)
- scale (x,y,z)
- parentId and child list

Parent/child bookkeeping is handled by Scene::SetParent and Scene::RemoveEntity.

## Components

### TransformComponent
- Simple TRS storage and parent/child links.

### MeshRendererComponent
- Visible flag and color (deprecated for UI, kept for fallback).
- Mesh asset ID and material asset ID.
- Rotation speed for demo animation.

### LightComponent
- Type: Directional, Point, Spot.
- Enabled, color, intensity, range, cone angles.
- Ambient color and "primary" flag.

### CameraComponent
- Projection type: Perspective or Orthographic.
- FOV, near/far clip, orthographic size, primary flag.

### RigidbodyComponent
- Motion type (Static/Kinematic/Dynamic), mass, damping.
- Gravity toggle, friction, restitution.
- Dirty flag for physics recreation and a BodyHandle.

### ColliderComponent
- Shape type (Box/Sphere/Capsule), size, radius, height.
- Trigger flag and offset.
- Dirty flag for physics recreation.

### SkeletonComponent
- Skeleton asset path (JSON skeleton file).
- Stores bone hierarchy and current pose.

### AnimatorComponent
- Animation clip library (clip name + source path).
- Global playback speed and root motion toggle.

### AudioSourceComponent
- Sound path, volume, pitch, looping, spatial, play-on-awake.
- Play/Stop requests and awake-play tracking.

### AudioListenerComponent
- Active flag (uses Transform for position/orientation).

## Scene Serialization
Files:
- `Engine/Scene/include/Aetherion/Scene/SceneSerializer.h`
- `Engine/Scene/src/SceneSerializer.cpp`

SceneSerializer reads/writes JSON:

Top-level:
- `name`
- `entities`: array

Entity object:
- `id`, `name`
- `components` object with named components:
  - Transform
    - `position` [x,y,z]
    - `rotation` [x,y,z]
    - `scale` [x,y,z]
    - `parent` (entity id)
  - MeshRenderer
    - `visible`, `color`, `rotationSpeed`, `meshId`, `materialId`
  - Light
    - `lightEnabled`, `lightType`, `lightColor`, `lightIntensity`, `lightRange`
    - `innerConeAngle`, `outerConeAngle`, `lightPrimary`, `ambientColor`
  - Camera
    - `projectionType`, `verticalFov`, `nearClip`, `farClip`,
      `orthographicSize`, `isPrimary`
  - Rigidbody
    - `motionType`, `mass`, `linearDamping`, `angularDamping`,
      `useGravity`, `friction`, `restitution`
  - Collider
    - `shapeType`, `halfExtents`, `radius`, `height`, `isTrigger`, `offset`
  - Skeleton
    - `skeletonPath`
  - Animator
    - `speed`, `rootMotion`
    - `clips`: array of `{ name, path }`
  - AudioSource
    - `soundPath`, `volume`, `pitch`, `loop`, `spatial`, `playOnAwake`
  - AudioListener
    - `active`

Backward compatibility:
- Transform accepts an older `rotationZ` field if `rotation` array is missing.

After load, parent/child lists are rebuilt from Transform parent IDs.
