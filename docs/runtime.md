# Runtime

## EngineContext
Files:
- `Engine/Runtime/include/Aetherion/Runtime/EngineContext.h`
- `Engine/Runtime/src/EngineContext.cpp`

EngineContext is a shared service container for runtime systems:
- VulkanContext, RenderView
- AssetRegistry
- PhysicsWorld
- AudioEngine
- ScriptingRuntimeStub

Simulation state is stored here:
- `SetSimulationState(playing, paused)`
- `RequestSimulationStep()` and `ConsumeSimulationStepRequest()`

## EngineApplication
Files:
- `Engine/Runtime/include/Aetherion/Runtime/EngineApplication.h`
- `Engine/Runtime/src/EngineApplication.cpp`

Responsibilities:
- Initialize Vulkan, asset registry, physics, audio, scripting.
- Resolve assets root:
  - `AETHERION_ASSETS_DIR` env var wins.
  - Falls back to `assets` folder near CWD or executable.
- Load project metadata from `aetherion.project.json` or `project.json`.
- Load bootstrap scene (or create/save a default scene if missing).
- Drive the main loop via `Run()` and `Tick()`.

Project metadata fields (optional):
- `name` / `projectName`
- `assetsRoot` / `assetsDir`
- `bootstrapScene` / `startupScene`

## Runtime Systems (in EngineApplication.cpp)
EngineApplication registers placeholder systems on startup:

- PhysicsRuntimeSystem
  - Ensures PhysicsWorld exists and initialized.
  - Wraps PhysicsSystem for scene sync.
  - Updates only when simulation is playing.
  - Supports step-once while paused.

- AudioRuntimeSystem
  - Wraps AudioSystem and binds the active scene.

- SceneSystemDispatcher
  - Calls `System::Configure()` once and `System::Update()` every tick.

- RenderViewSystem
  - Builds RenderView from the current Scene each tick.
  - Populates lights, cameras, colliders, and render instances.
  - Batches instances by MeshRenderer pointer.
  - Computes world transforms by walking Transform parent chains.
  - Supports animated "moving lights" by name for demo/debug.

## Simulation Control
EngineApplication exposes play/pause/step methods:
- `SetSimulationPlaying(bool)`
- `SetSimulationPaused(bool)`
- `StepSimulationOnce()`

These update EngineContext and are used by EditorMainWindow to control play mode.
