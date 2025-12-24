# Editor Usability Improvements

## Features

### Undo/Redo System
*   **Command History**: A robust Undo/Redo stack has been implemented.
*   **Actions Supported**:
    *   **Gizmo Transformations**: Moving, Rotating, and Scaling via the Viewport or Keyboard Shortcuts now creates Undoable commands.
    *   **Entity Operations**: Creating, Deleting, and Renaming entities (via Hierarchy context menu) are fully Undoable.
*   **UI**: "Undo" (Ctrl+Z) and "Redo" (Ctrl+Shift+Z or Ctrl+Y) are available in the Edit menu.

### Gizmos & Viewport Interaction
*   **Drag to Interact**: When a Gizmo mode (Move/Rotate/Scale) is active, dragging with the **Left Mouse Button** anywhere in the viewport modifies the selected entity.
    *   **Translate (W)**: Dragging moves the entity in World X/Y.
    *   **Rotate (E)**: Dragging horizontally rotates around the World Y axis.
    *   **Scale (R)**: Dragging vertically scales the entity uniformly.
*   **Command Merging**: Continuous dragging creates a single merged Undo entry, preventing stack pollution.

### Focus Selection
*   **Focus (F)**: Pressing 'F' or clicking the "Focus" button in the viewport centers the camera on the selected entity.
*   **Bounds Awareness**: The camera calculates the optimal distance based on the mesh's bounding box to ensure the object fills the view.

## Architecture

*   **Command Pattern**: Implemented in `Engine/Editor/include/Aetherion/Editor/Command.h`.
*   **CommandHistory**: Manages the undo/redo stacks (`Engine/Editor/include/Aetherion/Editor/CommandHistory.h`).
*   **Integration**: `EditorMainWindow` holds the `CommandHistory` and dispatches commands via `ExecuteCommand()`.

## Future Improvements
*   **Inspector Undo**: Hooks (`SetCommandExecutor`) have been added to `EditorInspectorPanel` to support Inspector-based Undo in the future, once UI rebuilding logic is optimized.
