#include "Aetherion/Platform/PlatformAbstraction.h"

#include <chrono>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <ShellScalingApi.h>
#endif

namespace Aetherion::Platform {

/// @brief Stub window implementation (placeholder until GLFW/SDL integration)
class StubWindow : public IWindow {
public:
  StubWindow(const WindowDescriptor &desc)
      : m_width(desc.width), m_height(desc.height), m_title(desc.title),
        m_fullscreen(desc.fullscreen), m_visible(desc.visible), m_open(true) {}

  ~StubWindow() override { m_open = false; }

  [[nodiscard]] bool IsOpen() const override { return m_open && m_visible; }

  void GetSize(int &width, int &height) const override {
    width = m_width;
    height = m_height;
  }

  void GetPosition(int &x, int &y) const override {
    x = m_x;
    y = m_y;
  }

  void SetSize(int width, int height) override {
    m_width = width;
    m_height = height;
    if (m_windowCallbacks.onWindowEvent) {
      WindowEvent event;
      event.type = WindowEventType::Resize;
      event.width = width;
      event.height = height;
      m_windowCallbacks.onWindowEvent(event);
    }
  }

  void SetPosition(int x, int y) override {
    m_x = x;
    m_y = y;
  }

  void SetTitle(const std::string &title) override { m_title = title; }

  void SetFullscreen(bool fullscreen) override { m_fullscreen = fullscreen; }

  [[nodiscard]] bool IsFullscreen() const override { return m_fullscreen; }

  void Minimize() override { m_minimized = true; }

  void Restore() override { m_minimized = false; }

  void Maximize() override { m_maximized = true; }

  void Focus() override { m_focused = true; }

  [[nodiscard]] bool HasFocus() const override { return m_focused; }

  void SetVisible(bool visible) override { m_visible = visible; }

  [[nodiscard]] NativeWindowHandle GetNativeHandle() const override {
    // Stub: return nullptr until native implementation
    return nullptr;
  }

  [[nodiscard]] DpiInfo GetDpi() const override {
    DpiInfo dpi;
#ifdef _WIN32
    // Try to get DPI on Windows
    HDC hdc = GetDC(nullptr);
    if (hdc) {
      dpi.dpiX = static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSX));
      dpi.dpiY = static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSY));
      dpi.scaleX = dpi.dpiX / 96.0f;
      dpi.scaleY = dpi.dpiY / 96.0f;
      ReleaseDC(nullptr, hdc);
    }
#endif
    return dpi;
  }

  VkSurfaceKHR CreateVulkanSurface(VkInstance instance) const override {
    // Stub: Vulkan surface creation requires native window handle
    // This would be implemented with platform-specific code or GLFW/SDL
    (void)instance;
    return nullptr;
  }

  void SetCursorShape(CursorShape shape) override { m_cursorShape = shape; }

  void SetCursorVisible(bool visible) override { m_cursorVisible = visible; }

  void SetCursorLocked(bool locked) override { m_cursorLocked = locked; }

  void SetInputCallbacks(const InputCallbacks &callbacks) override {
    m_inputCallbacks = callbacks;
  }

  void SetWindowCallbacks(const WindowCallbacks &callbacks) override {
    m_windowCallbacks = callbacks;
  }

  void PollEvents() override {
    // Stub: Would poll native events here
  }

  void SwapBuffers() override {
    // Stub: No-op for Vulkan, would swap for OpenGL
  }

  void RequestClose() override {
    if (m_windowCallbacks.onCloseRequest) {
      if (!m_windowCallbacks.onCloseRequest()) {
        return; // Close was prevented
      }
    }
    m_open = false;
    if (m_windowCallbacks.onWindowEvent) {
      WindowEvent event;
      event.type = WindowEventType::Close;
      m_windowCallbacks.onWindowEvent(event);
    }
  }

private:
  int m_width = 1280;
  int m_height = 720;
  int m_x = 100;
  int m_y = 100;
  std::string m_title;
  bool m_fullscreen = false;
  bool m_visible = true;
  bool m_open = true;
  bool m_minimized = false;
  bool m_maximized = false;
  bool m_focused = true;
  bool m_cursorVisible = true;
  bool m_cursorLocked = false;
  CursorShape m_cursorShape = CursorShape::Arrow;

  InputCallbacks m_inputCallbacks;
  WindowCallbacks m_windowCallbacks;
};

void PlatformAbstractionLayer::Initialize(const WindowDescriptor &descriptor) {
  if (m_initialized) {
    return;
  }

  m_descriptor = descriptor;

#ifdef _WIN32
  // Enable high DPI awareness on Windows
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

  // Create main window
  m_mainWindow = CreateWindow(descriptor);
  m_initialized = true;
}

void PlatformAbstractionLayer::Shutdown() {
  if (!m_initialized) {
    return;
  }

  m_mainWindow.reset();
  m_initialized = false;
}

std::unique_ptr<IWindow>
PlatformAbstractionLayer::CreateWindow(const WindowDescriptor &descriptor) {
  // TODO: Integrate GLFW, SDL, or native Win32/X11/Cocoa for real windows
  // For now, return a stub implementation
  return std::make_unique<StubWindow>(descriptor);
}

void PlatformAbstractionLayer::PollEvents() {
  if (m_mainWindow) {
    m_mainWindow->PollEvents();
  }
}

std::string PlatformAbstractionLayer::GetClipboardText() const {
#ifdef _WIN32
  if (!OpenClipboard(nullptr)) {
    return "";
  }

  HANDLE hData = GetClipboardData(CF_UNICODETEXT);
  if (!hData) {
    CloseClipboard();
    return "";
  }

  wchar_t *pszText = static_cast<wchar_t *>(GlobalLock(hData));
  if (!pszText) {
    CloseClipboard();
    return "";
  }

  // Convert wide string to UTF-8
  int size = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, nullptr, 0, nullptr, nullptr);
  std::string result(size - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, pszText, -1, result.data(), size, nullptr, nullptr);

  GlobalUnlock(hData);
  CloseClipboard();
  return result;
#else
  return "";
#endif
}

void PlatformAbstractionLayer::SetClipboardText(const std::string &text) {
#ifdef _WIN32
  if (!OpenClipboard(nullptr)) {
    return;
  }

  EmptyClipboard();

  // Convert UTF-8 to wide string
  int wsize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wsize * sizeof(wchar_t));
  if (!hMem) {
    CloseClipboard();
    return;
  }

  wchar_t *pMem = static_cast<wchar_t *>(GlobalLock(hMem));
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, wsize);
  GlobalUnlock(hMem);

  SetClipboardData(CF_UNICODETEXT, hMem);
  CloseClipboard();
#else
  (void)text;
#endif
}

FileDialogResult PlatformAbstractionLayer::ShowOpenFileDialog(
    const std::string &title, const std::vector<FileFilter> &filters,
    bool multiSelect) {
  FileDialogResult result;
  // TODO: Implement with native dialogs or portable-file-dialogs library
  (void)title;
  (void)filters;
  (void)multiSelect;
  return result;
}

FileDialogResult PlatformAbstractionLayer::ShowSaveFileDialog(
    const std::string &title, const std::vector<FileFilter> &filters,
    const std::string &defaultName) {
  FileDialogResult result;
  // TODO: Implement with native dialogs
  (void)title;
  (void)filters;
  (void)defaultName;
  return result;
}

FileDialogResult
PlatformAbstractionLayer::ShowFolderPickerDialog(const std::string &title) {
  FileDialogResult result;
  // TODO: Implement with native dialogs
  (void)title;
  return result;
}

double PlatformAbstractionLayer::GetTime() const {
  auto now = std::chrono::system_clock::now();
  auto epoch = now.time_since_epoch();
  return std::chrono::duration<double>(epoch).count();
}

double PlatformAbstractionLayer::GetHighResTime() const {
  auto now = std::chrono::high_resolution_clock::now();
  auto epoch = now.time_since_epoch();
  return std::chrono::duration<double>(epoch).count();
}

int PlatformAbstractionLayer::GetCpuCount() const {
  return static_cast<int>(std::thread::hardware_concurrency());
}

uint64_t PlatformAbstractionLayer::GetSystemMemory() const {
#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  return static_cast<uint64_t>(memInfo.ullTotalPhys);
#else
  return 0; // TODO: Implement for other platforms
#endif
}

} // namespace Aetherion::Platform
