#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for Vulkan
typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;

namespace Aetherion::Platform {

/// @brief Window creation settings
struct WindowDescriptor {
  int width = 1280;
  int height = 720;
  std::string title = "Aetherion";
  bool fullscreen = false;
  bool resizable = true;
  bool vsync = true;
  bool highDpi = true;
  bool visible = true;
  bool decorated = true;  // Window decorations (title bar, borders)
};

/// @brief DPI/scale information
struct DpiInfo {
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float dpiX = 96.0f;
  float dpiY = 96.0f;
};

/// @brief Window event types
enum class WindowEventType : uint8_t {
  None = 0,
  Resize,
  Close,
  Focus,
  Blur,
  Minimize,
  Restore,
  DpiChange,
  Move
};

/// @brief Window event data
struct WindowEvent {
  WindowEventType type = WindowEventType::None;
  int width = 0;
  int height = 0;
  int x = 0;
  int y = 0;
  float dpiScale = 1.0f;
};

/// @brief Keyboard key codes
enum class KeyCode : uint16_t {
  Unknown = 0,
  // Letters
  A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  // Numbers
  Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
  // Function keys
  F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
  // Special keys
  Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
  Right = 262, Left = 263, Down = 264, Up = 265,
  PageUp = 266, PageDown = 267, Home = 268, End = 269,
  CapsLock = 280, ScrollLock = 281, NumLock = 282,
  PrintScreen = 283, Pause = 284,
  Space = 32,
  // Modifiers
  LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
  RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347
};

/// @brief Mouse button codes
enum class MouseButton : uint8_t {
  Left = 0,
  Right = 1,
  Middle = 2,
  Button4 = 3,
  Button5 = 4
};

/// @brief Keyboard event data
struct KeyEvent {
  KeyCode key = KeyCode::Unknown;
  bool pressed = false;
  bool repeat = false;
  bool shift = false;
  bool control = false;
  bool alt = false;
  bool super = false;
};

/// @brief Mouse button event data
struct MouseButtonEvent {
  MouseButton button = MouseButton::Left;
  bool pressed = false;
  float x = 0.0f;
  float y = 0.0f;
};

/// @brief Mouse move event data
struct MouseMoveEvent {
  float x = 0.0f;
  float y = 0.0f;
  float deltaX = 0.0f;
  float deltaY = 0.0f;
};

/// @brief Mouse scroll event data
struct MouseScrollEvent {
  float deltaX = 0.0f;
  float deltaY = 0.0f;
};

/// @brief Cursor shape
enum class CursorShape : uint8_t {
  Arrow = 0,
  IBeam,
  Crosshair,
  Hand,
  ResizeEW,
  ResizeNS,
  ResizeNWSE,
  ResizeNESW,
  ResizeAll,
  NotAllowed,
  Hidden
};

/// @brief Input event callbacks
struct InputCallbacks {
  std::function<void(const KeyEvent &)> onKey;
  std::function<void(const MouseButtonEvent &)> onMouseButton;
  std::function<void(const MouseMoveEvent &)> onMouseMove;
  std::function<void(const MouseScrollEvent &)> onMouseScroll;
  std::function<void(unsigned int codepoint)> onChar;
};

/// @brief Window event callbacks
struct WindowCallbacks {
  std::function<void(const WindowEvent &)> onWindowEvent;
  std::function<bool()> onCloseRequest;  // Return false to prevent close
};

/// @brief Opaque window handle
using NativeWindowHandle = void *;

/// @brief Abstract window interface
class IWindow {
public:
  virtual ~IWindow() = default;

  /// @brief Check if window is valid and open
  [[nodiscard]] virtual bool IsOpen() const = 0;

  /// @brief Get current window size
  virtual void GetSize(int &width, int &height) const = 0;

  /// @brief Get current window position
  virtual void GetPosition(int &x, int &y) const = 0;

  /// @brief Set window size
  virtual void SetSize(int width, int height) = 0;

  /// @brief Set window position
  virtual void SetPosition(int x, int y) = 0;

  /// @brief Set window title
  virtual void SetTitle(const std::string &title) = 0;

  /// @brief Set fullscreen mode
  virtual void SetFullscreen(bool fullscreen) = 0;

  /// @brief Check if fullscreen
  [[nodiscard]] virtual bool IsFullscreen() const = 0;

  /// @brief Minimize window
  virtual void Minimize() = 0;

  /// @brief Restore window from minimized state
  virtual void Restore() = 0;

  /// @brief Maximize window
  virtual void Maximize() = 0;

  /// @brief Focus window
  virtual void Focus() = 0;

  /// @brief Check if window has focus
  [[nodiscard]] virtual bool HasFocus() const = 0;

  /// @brief Show/hide window
  virtual void SetVisible(bool visible) = 0;

  /// @brief Get native window handle (HWND on Windows, etc.)
  [[nodiscard]] virtual NativeWindowHandle GetNativeHandle() const = 0;

  /// @brief Get DPI information
  [[nodiscard]] virtual DpiInfo GetDpi() const = 0;

  /// @brief Create Vulkan surface for this window
  virtual VkSurfaceKHR CreateVulkanSurface(VkInstance instance) const = 0;

  /// @brief Set cursor shape
  virtual void SetCursorShape(CursorShape shape) = 0;

  /// @brief Set cursor visibility
  virtual void SetCursorVisible(bool visible) = 0;

  /// @brief Lock cursor to window (for FPS controls)
  virtual void SetCursorLocked(bool locked) = 0;

  /// @brief Set input callbacks
  virtual void SetInputCallbacks(const InputCallbacks &callbacks) = 0;

  /// @brief Set window callbacks
  virtual void SetWindowCallbacks(const WindowCallbacks &callbacks) = 0;

  /// @brief Poll events (call each frame)
  virtual void PollEvents() = 0;

  /// @brief Swap buffers (if using OpenGL, otherwise no-op for Vulkan)
  virtual void SwapBuffers() = 0;

  /// @brief Request window close
  virtual void RequestClose() = 0;
};

/// @brief File dialog result
struct FileDialogResult {
  bool success = false;
  std::vector<std::string> paths;
};

/// @brief File dialog filter
struct FileFilter {
  std::string name;        // e.g., "Images"
  std::string extensions;  // e.g., "*.png;*.jpg;*.jpeg"
};

/// @brief Platform abstraction layer
///
/// Provides OS-independent access to:
/// - Window creation and management
/// - Input handling
/// - File dialogs
/// - Clipboard
/// - System information
class PlatformAbstractionLayer {
public:
  PlatformAbstractionLayer() = default;
  ~PlatformAbstractionLayer() = default;

  /// @brief Initialize the platform layer
  /// @param descriptor Window settings
  void Initialize(const WindowDescriptor &descriptor);

  /// @brief Shutdown and release all resources
  void Shutdown();

  /// @brief Check if initialized
  [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

  /// @brief Create a new window
  /// @param descriptor Window settings
  /// @return Window instance or nullptr on failure
  [[nodiscard]] std::unique_ptr<IWindow> CreateWindow(
      const WindowDescriptor &descriptor);

  /// @brief Get the main/primary window
  [[nodiscard]] IWindow *GetMainWindow() noexcept { return m_mainWindow.get(); }

  /// @brief Poll events for all windows
  void PollEvents();

  /// @brief Get clipboard text
  [[nodiscard]] std::string GetClipboardText() const;

  /// @brief Set clipboard text
  void SetClipboardText(const std::string &text);

  /// @brief Show open file dialog
  [[nodiscard]] FileDialogResult ShowOpenFileDialog(
      const std::string &title,
      const std::vector<FileFilter> &filters = {},
      bool multiSelect = false);

  /// @brief Show save file dialog
  [[nodiscard]] FileDialogResult ShowSaveFileDialog(
      const std::string &title,
      const std::vector<FileFilter> &filters = {},
      const std::string &defaultName = "");

  /// @brief Show folder picker dialog
  [[nodiscard]] FileDialogResult ShowFolderPickerDialog(
      const std::string &title);

  /// @brief Get system time in seconds since epoch
  [[nodiscard]] double GetTime() const;

  /// @brief Get high-resolution timer in seconds
  [[nodiscard]] double GetHighResTime() const;

  /// @brief Get number of logical CPU cores
  [[nodiscard]] int GetCpuCount() const;

  /// @brief Get system memory in bytes
  [[nodiscard]] uint64_t GetSystemMemory() const;

  /// @brief Get the stored window descriptor
  [[nodiscard]] const WindowDescriptor &GetDescriptor() const noexcept {
    return m_descriptor;
  }

private:
  WindowDescriptor m_descriptor;
  std::unique_ptr<IWindow> m_mainWindow;
  bool m_initialized = false;
};

} // namespace Aetherion::Platform
