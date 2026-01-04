# Editor

## EditorApplication
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorApplication.h`
- `Engine/Editor/src/EditorApplication.cpp`

- Creates QApplication and loads EditorSettings.
- Honors `AETHERION_ENABLE_VK_VALIDATION` env var for validation toggles.
- Initializes EngineApplication and builds the main window.

## EditorMainWindow
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorMainWindow.h`
- `Engine/Editor/src/EditorMainWindow.cpp`

Key responsibilities:
- Owns the runtime app and active Scene.
- Hosts the VulkanViewport and drives the render loop via QTimer.
- Routes selection from picking results back into EditorSelection.
- Manages play/pause/step controls and scene snapshots for play sessions.
- Polls AssetRegistry change log and refreshes editor UI and GPU caches.
- Handles scene load/save via SceneSerializer.
- Provides undo/redo via CommandHistory.
- Maintains gizmo mode (translate/rotate/scale) with snapping.

### Quick Info Panel
EditorAuxPanel (private class) displays scene/selection/asset/camera status.

## EditorViewport
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorViewport.h`
- `Engine/Editor/src/EditorViewport.cpp`

- Native QWidget surface for Vulkan rendering.
- Mouse controls for orbit, pan, and zoom; WASD/QE fly movement.
- Emits `surfaceReady`, `surfaceResized`, and `cameraChanged` signals.
- Provides a Focus button and shortcut hints overlay.

## EditorHierarchyPanel
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorHierarchyPanel.h`
- `Engine/Editor/src/EditorHierarchyPanel.cpp`

- Tree view of entities with drag-and-drop reparenting.
- Context menu actions: create, duplicate, rename, delete.
- Emits selection and reparent events to EditorMainWindow.

## EditorInspectorPanel
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorInspectorPanel.h`
- `Engine/Editor/src/EditorInspectorPanel.cpp`

- Dynamically rebuilds UI for selected entity or asset.
- Edits Transform, MeshRenderer, Light, Camera, Rigidbody, Collider,
  AudioSource components.
- Emits `transformChanged` and `sceneModified` signals.
- Uses CommandExecutor to push undoable commands (Transform/Component edits).
- For assets, surfaces mesh stats, dependency lists, and import settings.

## EditorAssetBrowser
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorAssetBrowser.h`
- `Engine/Editor/src/EditorAssetBrowser.cpp`

- List + filter view with back/forward navigation.
- Drag-and-drop to viewport to add assets to scene.
- Context actions: copy ID/path, show in explorer, rename, delete, rescan.

## EditorMeshPreview / EditorCameraPreview
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorMeshPreview.h`
- `Engine/Editor/src/EditorMeshPreview.cpp`
- `Engine/Editor/include/Aetherion/Editor/EditorCameraPreview.h`
- `Engine/Editor/src/EditorCameraPreview.cpp`

Both panels host their own VulkanViewport:
- MeshPreview renders a single mesh asset with orbit controls and auto-fit.
- CameraPreview renders the selected or primary camera view from RenderView.

## EditorConsole
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorConsole.h`
- `Engine/Editor/src/EditorConsole.cpp`

- Log view with severity filters, search, auto-scroll, clear, and copy.

## Selection
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorSelection.h`
- `Engine/Editor/src/EditorSelection.cpp`

- Tracks active scene and selected entity.
- Emits SelectionChanged / SelectionCleared.

## Commands and Undo/Redo
Files:
- `Engine/Editor/include/Aetherion/Editor/Command.h`
- `Engine/Editor/include/Aetherion/Editor/CommandHistory.h`
- `Engine/Editor/include/Aetherion/Editor/Commands/*`

CommandHistory executes and stores commands for undo/redo:
- TransformCommand (mergeable)
- CreateEntityCommand, DeleteEntityCommand, RenameEntityCommand
- AddComponentCommand, RemoveComponentCommand

## AI Copilot
Files:
- `Engine/Editor/include/Aetherion/Editor/AICopilotPanel.h`
- `Engine/Editor/src/AICopilotPanel.cpp`
- `Engine/Editor/include/Aetherion/Editor/AICopilotProcessor.h`
- `Engine/Editor/src/AICopilotProcessor.cpp`

- Simple text UI for prompts.
- Processor supports spawning entities (lights/cameras/cubes) and grid layouts.
- Uses CommandExecutor to add entities into the Scene.

## Settings
Files:
- `Engine/Editor/include/Aetherion/Editor/EditorSettings.h`
- `Engine/Editor/src/EditorSettings.cpp`
- `Engine/Editor/include/Aetherion/Editor/EditorSettingsDialog.h`
- `Engine/Editor/src/EditorSettingsDialog.cpp`

Settings stored in QSettings:
- validationEnabled
- verboseLogging
- targetFps
- headlessSleepMs
