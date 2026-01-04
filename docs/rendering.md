# Rendering

## RenderView Data Model
File: `Engine/Rendering/include/Aetherion/Rendering/RenderView.h`

RenderView is a CPU-side snapshot built each tick:
- RenderInstance: entity id, pointers to Transform/MeshRenderer, asset IDs,
  optional precomputed model matrix.
- RenderBatch: grouped instances (currently by mesh pointer).
- Lights: directional light + list of lights (point/spot/directional).
- Cameras: primary camera + list of cameras.
- Colliders: debug data for physics visualization.
- Materials: resolved material records for GPU binding.

## Renderer Reality
Current renderer scope:
- Vulkan viewport with opaque + picking + post-process passes.
- Basic PBR-ish lighting with a small light budget.
- No neural baking, denoisers, NeRF, or DLSS/XeSS integration yet.

## Roadmap Hooks
Planned extensions required by AI-native rendering claims:
- RenderView extensions for inference-backed passes and debug outputs.
- AssetRegistry support for latent/virtual textures and cache policies.
- Editor tooling to toggle neural passes and capture training data.

## VulkanContext
Files:
- `Engine/Rendering/include/Aetherion/Rendering/VulkanContext.h`
- `Engine/Rendering/src/VulkanContext.cpp`

Responsibilities:
- Create/destroy VkInstance, physical device, logical device, queues.
- Optional validation layers and debug messenger.
- Query swapchain support and queue families.
- Log device info when logging is enabled.

Important methods:
- Initialize(enableValidation, enableLogging)
- EnsureSurfaceCompatibility(surface)
- QuerySwapchainSupport(surface)

## VulkanViewport
Files:
- `Engine/Rendering/include/Aetherion/Rendering/VulkanViewport.h`
- `Engine/Rendering/src/VulkanViewport.cpp`

VulkanViewport is a self-contained renderer for a native window handle.

### Initialization Flow
- Create surface (platform-specific).
- Create swapchain and render passes.
- Allocate descriptor layouts/pools, buffers, and pipelines.
- Create scene, picking, and post-process resources.

### Render Passes
FrameStats exposes 4 named passes:
- Opaque
- Picking
- PostProcess
- Overlay

GPU timing queries are recorded per pass when supported.

### Picking
- RequestPick(x, y) queues a pick.
- A picking render target stores entity IDs.
- Results are read back to `PickResult` and consumed by the editor.

### Debug View
DebugViewMode toggles which GBuffer output is shown:
Final, Normals, Roughness, Metallic, Albedo, Depth, EntityId.

### Asset Integration
- Uses AssetRegistry to resolve meshes, textures, and materials.
- HandleAssetChanges invalidates GPU caches for changed assets.

### Camera Controls (editor)
VulkanViewport exposes camera utilities used by editor panels:
- SetCameraPosition/Rotation/Zoom/Distance
- FocusOnBounds(center, radius, padding)

### Shaders
File: `Engine/Rendering/shaders/*`
- viewport_triangle.vert/frag: full-screen triangle for post-processing.
- viewport_postprocess.vert/frag: post-process pass (tone mapping, etc.).
- viewport_picking.frag/viewport_picking_uint.frag: ID buffer output.
- viewport_picking_uint.frag: uint ID buffer variant.
- viewport_postprocess_uint.frag: uint post-process path.

---

## Renderer Reality

This section clarifies what the renderer actually supports today, to avoid
confusion with the visionary roadmap.

### What Works
- **Vulkan backend**: instance, device, swapchain, render passes.
- **Render passes**: Opaque geometry, entity-ID picking, post-process, overlay.
- **GBuffer debug views**: Albedo, Normals, Roughness, Metallic, Depth, EntityId.
- **Material binding**: PBR materials with albedo, normal, metallic-roughness maps.
- **Asset integration**: Meshes and textures loaded from AssetRegistry.
- **Camera controls**: Orbit, pan, zoom for editor viewport.

### What Does NOT Work Yet
- **Neural baking / NeRF**: No neural radiance field integration.
- **DLSS / FSR**: No AI upscaling hooks.
- **Ray tracing**: No RT pipeline or acceleration structures.
- **Transparent / alpha-blended geometry**: Not implemented.
- **Shadow mapping**: Not implemented.
- **Global illumination**: Not implemented.
- **sRGB correctness**: Albedo textures may not be sampled as sRGB (see asset-pipeline.md).

### Limitations
- Single directional light only.
- Fixed-function tone mapping (no configurable post-process chain).
- No LOD or culling beyond basic frustum.

---

## Roadmap Hooks

These are the APIs and extension points required for future AI-native rendering
features.

### For Neural Baking
- `IRenderPassExtension` interface to inject custom render passes.
- Compute shader dispatch API for neural network inference.
- GBuffer export for training data collection.

### For DLSS / FSR
- Pre-upscale render target at reduced resolution.
- Motion vector buffer generation.
- Integration point in post-process chain before final output.

### For AI Asset Generation
- Runtime texture upload API for generated content.
- Material hot-swap without full pipeline rebuild.
- Progress/streaming texture updates for iterative generation.

### For Semantic Scene Queries
- Per-object metadata buffer (semantic class, importance weight).
- GPU-side scene graph queries via compute shaders.
- Debug visualization for semantic labels.
