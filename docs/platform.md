# Platform Abstraction

Files:
- `Engine/Platform/include/Aetherion/Platform/PlatformAbstraction.h`
- `Engine/Platform/src/PlatformAbstraction.cpp`

## Current State

`PlatformAbstractionLayer` is a placeholder for windowing and OS services.

`WindowDescriptor` includes:
- width, height
- title

Initialize/Shutdown currently store the descriptor only (no OS integration yet).

---

## Windowing & Input Milestone List

This section tracks the concrete steps to make the Platform layer functional
for standalone runtime (non-Qt) execution.

### Milestone 1: Window Creation
- [ ] Create native window on Windows (Win32), Linux (X11/Wayland), macOS (Cocoa).
- [ ] Return opaque window handle (`HWND`, `Window`, `NSWindow*`).
- [ ] Support initial size, title, and optional fullscreen flag.
- [ ] Expose `GetNativeHandle()` for Vulkan surface creation.

### Milestone 2: Window Events
- [ ] Resize callback with new width/height.
- [ ] Close request callback (user clicks X).
- [ ] Focus gained/lost callbacks.
- [ ] Minimize/restore callbacks.

### Milestone 3: DPI Awareness
- [ ] Query initial DPI/scale factor.
- [ ] DPI change callback for per-monitor DPI.
- [ ] Expose logical vs physical pixel conversion.

### Milestone 4: Swapchain Surface
- [ ] `CreateVulkanSurface(VkInstance)` returns `VkSurfaceKHR`.
- [ ] Handle surface recreation on resize.
- [ ] Vsync toggle via present mode selection.

### Milestone 5: Input Handling
- [ ] Keyboard key down/up events with keycodes.
- [ ] Mouse button down/up events.
- [ ] Mouse move events (absolute and delta).
- [ ] Mouse scroll events.
- [ ] Gamepad/controller support (optional).

### Milestone 6: Cursor & Clipboard
- [ ] Set cursor visibility (hidden for FPS controls).
- [ ] Set cursor shape (arrow, hand, resize, etc.).
- [ ] Clipboard read/write for text.

### Milestone 7: File Dialogs (Optional)
- [ ] Open file dialog.
- [ ] Save file dialog.
- [ ] Folder picker.

### Implementation Notes
- The editor uses Qt for windowing; this layer is for standalone runtime builds.
- Consider wrapping SDL2 or GLFW for cross-platform coverage.
- Alternatively, implement minimal native code per platform.

### Dependencies
- Vulkan surface creation requires window handle.
- Renderer resize requires window resize callback.
- Input is needed for in-game controls and editor shortcuts.
