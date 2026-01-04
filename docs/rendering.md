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
