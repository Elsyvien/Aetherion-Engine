# Engine Overview

Aetherion is a modular C++20 engine with a Qt 6 editor and a Vulkan renderer.
The repository already contains a working editor and runtime loop; some systems
remain placeholders (audio, scripting, and parts of physics/renderer pipelines
are still evolving).

## Module Map
- Engine/Core: basic types, logging, math, UUIDs.
- Engine/Runtime: EngineApplication main loop, EngineContext service container.
- Engine/Scene: entity/component scene graph and JSON serialization.
- Engine/Assets: asset registry, metadata, import settings, materials.
- Engine/Rendering: VulkanContext + VulkanViewport and RenderView data model.
- Engine/Physics: Jolt-backed PhysicsWorld and a scene sync system.
- Engine/Audio: miniaudio-backed AudioEngine and simple AudioSystem.
- Engine/Editor: Qt widgets for editing, selection, and rendering integration.
- Engine/Platform: minimal window abstraction (placeholder).
- Engine/Scripting: placeholder runtime stub.

## High-Level Data Flow
Editor (Qt) drives the runtime loop and renderer:

1) EditorMainWindow -> EngineApplication::Tick()
2) EngineApplication -> RenderViewSystem rebuilds RenderView from Scene
3) EditorMainWindow -> VulkanViewport::RenderFrame(RenderView)
4) VulkanViewport pulls assets from AssetRegistry caches
5) Editor uses pick results to update EditorSelection

Simulation (play/pause/step) is handled in EngineApplication and propagated
through EngineContext to physics and scene updates.

## Asset and Scene Flow
- AssetRegistry scans the assets root and assigns stable asset IDs via
  per-asset `.asset.json` metadata files.
- SceneSerializer stores scene JSON with entity/component data and asset IDs.
- Runtime loads a bootstrap scene from `assets/scenes/bootstrap_scene.json`
  (or a project-specified path from `aetherion.project.json`).
