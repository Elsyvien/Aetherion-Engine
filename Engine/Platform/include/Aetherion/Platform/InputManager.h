#pragma once

#include "Aetherion/Platform/PlatformAbstraction.h"

#include <array>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace Aetherion::Platform {

/// @brief Input action state
enum class InputAction : uint8_t { Released = 0, Pressed = 1, Repeat = 2 };

/// @brief Input axis (for analog input like gamepad sticks)
struct InputAxis {
  float x{0.0f};
  float y{0.0f};

  [[nodiscard]] float Magnitude() const noexcept {
    return std::sqrt(x * x + y * y);
  }
  [[nodiscard]] InputAxis Normalized() const noexcept {
    const float mag = Magnitude();
    return mag > 0.001f ? InputAxis{x / mag, y / mag} : InputAxis{0.0f, 0.0f};
  }
};

/// @brief Keyboard input event
struct KeyEvent {
  KeyCode key{KeyCode::Unknown};
  InputAction action{InputAction::Released};
  bool shift{false};
  bool ctrl{false};
  bool alt{false};
  bool super{false};
};

/// @brief Mouse button input event
struct MouseButtonEvent {
  MouseButton button{MouseButton::Left};
  InputAction action{InputAction::Released};
  float x{0.0f};
  float y{0.0f};
  bool shift{false};
  bool ctrl{false};
  bool alt{false};
};

/// @brief Mouse move event
struct MouseMoveEvent {
  float x{0.0f};
  float y{0.0f};
  float deltaX{0.0f};
  float deltaY{0.0f};
};

/// @brief Mouse scroll event
struct MouseScrollEvent {
  float offsetX{0.0f};
  float offsetY{0.0f};
};

/// @brief Named input action binding
struct InputBinding {
  std::string name;
  std::vector<KeyCode> keys;
  std::vector<MouseButton> mouseButtons;
  bool consumed{false};
};

/// @brief Manages keyboard, mouse and gamepad input with buffering and action
/// mapping
///
/// @code
/// InputManager input;
///
/// // Register action bindings
/// input.RegisterAction("Jump", {KeyCode::Space});
/// input.RegisterAction("Shoot", {}, {MouseButton::Left});
///
/// // In update loop
/// if (input.IsActionPressed("Jump")) {
///   player.Jump();
/// }
///
/// if (input.IsKeyDown(KeyCode::W)) {
///   player.MoveForward();
/// }
///
/// // Get mouse delta for camera
/// auto [dx, dy] = input.GetMouseDelta();
/// camera.Rotate(dx, dy);
///
/// // End of frame
/// input.EndFrame();
/// @endcode
class InputManager {
public:
  using KeyCallback = std::function<void(const KeyEvent &)>;
  using MouseButtonCallback = std::function<void(const MouseButtonEvent &)>;
  using MouseMoveCallback = std::function<void(const MouseMoveEvent &)>;
  using MouseScrollCallback = std::function<void(const MouseScrollEvent &)>;

  InputManager() {
    m_keyStates.fill(false);
    m_previousKeyStates.fill(false);
    m_mouseButtonStates.fill(false);
    m_previousMouseButtonStates.fill(false);
  }

  // =========================================================================
  // Input Event Injection (called by window/platform layer)
  // =========================================================================

  /// @brief Process a key event from the platform
  void OnKeyEvent(const KeyEvent &event) {
    std::lock_guard lock(m_mutex);

    const auto keyIndex = static_cast<size_t>(event.key);
    if (keyIndex < m_keyStates.size()) {
      m_keyStates[keyIndex] = (event.action != InputAction::Released);
    }

    m_keyEvents.push(event);

    for (auto &callback : m_keyCallbacks) {
      callback(event);
    }
  }

  /// @brief Process a mouse button event from the platform
  void OnMouseButtonEvent(const MouseButtonEvent &event) {
    std::lock_guard lock(m_mutex);

    const auto buttonIndex = static_cast<size_t>(event.button);
    if (buttonIndex < m_mouseButtonStates.size()) {
      m_mouseButtonStates[buttonIndex] =
          (event.action != InputAction::Released);
    }

    m_mouseButtonEvents.push(event);

    for (auto &callback : m_mouseButtonCallbacks) {
      callback(event);
    }
  }

  /// @brief Process a mouse move event from the platform
  void OnMouseMoveEvent(const MouseMoveEvent &event) {
    std::lock_guard lock(m_mutex);

    m_mouseDeltaX += event.deltaX;
    m_mouseDeltaY += event.deltaY;
    m_mouseX = event.x;
    m_mouseY = event.y;

    for (auto &callback : m_mouseMoveCallbacks) {
      callback(event);
    }
  }

  /// @brief Process a mouse scroll event from the platform
  void OnMouseScrollEvent(const MouseScrollEvent &event) {
    std::lock_guard lock(m_mutex);

    m_scrollDeltaX += event.offsetX;
    m_scrollDeltaY += event.offsetY;

    for (auto &callback : m_mouseScrollCallbacks) {
      callback(event);
    }
  }

  // =========================================================================
  // Key State Queries
  // =========================================================================

  /// @brief Check if a key is currently held down
  [[nodiscard]] bool IsKeyDown(KeyCode key) const noexcept {
    const auto index = static_cast<size_t>(key);
    return index < m_keyStates.size() && m_keyStates[index];
  }

  /// @brief Check if a key was just pressed this frame
  [[nodiscard]] bool IsKeyPressed(KeyCode key) const noexcept {
    const auto index = static_cast<size_t>(key);
    return index < m_keyStates.size() && m_keyStates[index] &&
           !m_previousKeyStates[index];
  }

  /// @brief Check if a key was just released this frame
  [[nodiscard]] bool IsKeyReleased(KeyCode key) const noexcept {
    const auto index = static_cast<size_t>(key);
    return index < m_keyStates.size() && !m_keyStates[index] &&
           m_previousKeyStates[index];
  }

  // =========================================================================
  // Mouse State Queries
  // =========================================================================

  /// @brief Check if a mouse button is currently held down
  [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const noexcept {
    const auto index = static_cast<size_t>(button);
    return index < m_mouseButtonStates.size() && m_mouseButtonStates[index];
  }

  /// @brief Check if a mouse button was just pressed this frame
  [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const noexcept {
    const auto index = static_cast<size_t>(button);
    return index < m_mouseButtonStates.size() && m_mouseButtonStates[index] &&
           !m_previousMouseButtonStates[index];
  }

  /// @brief Check if a mouse button was just released this frame
  [[nodiscard]] bool IsMouseButtonReleased(MouseButton button) const noexcept {
    const auto index = static_cast<size_t>(button);
    return index < m_mouseButtonStates.size() && !m_mouseButtonStates[index] &&
           m_previousMouseButtonStates[index];
  }

  /// @brief Get current mouse position
  [[nodiscard]] std::pair<float, float> GetMousePosition() const noexcept {
    return {m_mouseX, m_mouseY};
  }

  /// @brief Get mouse delta since last frame
  [[nodiscard]] std::pair<float, float> GetMouseDelta() const noexcept {
    return {m_mouseDeltaX, m_mouseDeltaY};
  }

  /// @brief Get scroll delta since last frame
  [[nodiscard]] std::pair<float, float> GetScrollDelta() const noexcept {
    return {m_scrollDeltaX, m_scrollDeltaY};
  }

  // =========================================================================
  // Action Mapping
  // =========================================================================

  /// @brief Register a named input action
  void RegisterAction(const std::string &name,
                      std::vector<KeyCode> keys = {},
                      std::vector<MouseButton> mouseButtons = {}) {
    std::lock_guard lock(m_mutex);
    m_actionBindings[name] = InputBinding{name, std::move(keys),
                                          std::move(mouseButtons), false};
  }

  /// @brief Remove a named input action
  void UnregisterAction(const std::string &name) {
    std::lock_guard lock(m_mutex);
    m_actionBindings.erase(name);
  }

  /// @brief Check if an action is currently active (any bound key/button down)
  [[nodiscard]] bool IsActionDown(const std::string &name) const {
    std::lock_guard lock(m_mutex);
    auto it = m_actionBindings.find(name);
    if (it == m_actionBindings.end())
      return false;

    const auto &binding = it->second;

    for (const auto &key : binding.keys) {
      if (IsKeyDown(key))
        return true;
    }

    for (const auto &button : binding.mouseButtons) {
      if (IsMouseButtonDown(button))
        return true;
    }

    return false;
  }

  /// @brief Check if an action was just pressed this frame
  [[nodiscard]] bool IsActionPressed(const std::string &name) const {
    std::lock_guard lock(m_mutex);
    auto it = m_actionBindings.find(name);
    if (it == m_actionBindings.end())
      return false;

    const auto &binding = it->second;

    for (const auto &key : binding.keys) {
      if (IsKeyPressed(key))
        return true;
    }

    for (const auto &button : binding.mouseButtons) {
      if (IsMouseButtonPressed(button))
        return true;
    }

    return false;
  }

  /// @brief Check if an action was just released this frame
  [[nodiscard]] bool IsActionReleased(const std::string &name) const {
    std::lock_guard lock(m_mutex);
    auto it = m_actionBindings.find(name);
    if (it == m_actionBindings.end())
      return false;

    const auto &binding = it->second;

    for (const auto &key : binding.keys) {
      if (IsKeyReleased(key))
        return true;
    }

    for (const auto &button : binding.mouseButtons) {
      if (IsMouseButtonReleased(button))
        return true;
    }

    return false;
  }

  /// @brief Get 2D axis from WASD or arrow keys
  [[nodiscard]] InputAxis GetMovementAxis() const noexcept {
    InputAxis axis;

    if (IsKeyDown(KeyCode::W) || IsKeyDown(KeyCode::Up))
      axis.y += 1.0f;
    if (IsKeyDown(KeyCode::S) || IsKeyDown(KeyCode::Down))
      axis.y -= 1.0f;
    if (IsKeyDown(KeyCode::A) || IsKeyDown(KeyCode::Left))
      axis.x -= 1.0f;
    if (IsKeyDown(KeyCode::D) || IsKeyDown(KeyCode::Right))
      axis.x += 1.0f;

    // Normalize diagonal movement
    const float mag = axis.Magnitude();
    if (mag > 1.0f) {
      axis.x /= mag;
      axis.y /= mag;
    }

    return axis;
  }

  // =========================================================================
  // Callbacks
  // =========================================================================

  /// @brief Add a key event callback
  void AddKeyCallback(KeyCallback callback) {
    std::lock_guard lock(m_mutex);
    m_keyCallbacks.push_back(std::move(callback));
  }

  /// @brief Add a mouse button event callback
  void AddMouseButtonCallback(MouseButtonCallback callback) {
    std::lock_guard lock(m_mutex);
    m_mouseButtonCallbacks.push_back(std::move(callback));
  }

  /// @brief Add a mouse move event callback
  void AddMouseMoveCallback(MouseMoveCallback callback) {
    std::lock_guard lock(m_mutex);
    m_mouseMoveCallbacks.push_back(std::move(callback));
  }

  /// @brief Add a mouse scroll event callback
  void AddMouseScrollCallback(MouseScrollCallback callback) {
    std::lock_guard lock(m_mutex);
    m_mouseScrollCallbacks.push_back(std::move(callback));
  }

  // =========================================================================
  // Frame Management
  // =========================================================================

  /// @brief Call at the end of each frame to update previous states
  void EndFrame() {
    std::lock_guard lock(m_mutex);

    m_previousKeyStates = m_keyStates;
    m_previousMouseButtonStates = m_mouseButtonStates;

    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;

    // Clear event queues
    while (!m_keyEvents.empty())
      m_keyEvents.pop();
    while (!m_mouseButtonEvents.empty())
      m_mouseButtonEvents.pop();
  }

  /// @brief Reset all input state
  void Reset() {
    std::lock_guard lock(m_mutex);

    m_keyStates.fill(false);
    m_previousKeyStates.fill(false);
    m_mouseButtonStates.fill(false);
    m_previousMouseButtonStates.fill(false);

    m_mouseX = 0.0f;
    m_mouseY = 0.0f;
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;

    while (!m_keyEvents.empty())
      m_keyEvents.pop();
    while (!m_mouseButtonEvents.empty())
      m_mouseButtonEvents.pop();
  }

private:
  mutable std::mutex m_mutex;

  // Key state (256 keys should cover most keyboards)
  static constexpr size_t kMaxKeys = 256;
  std::array<bool, kMaxKeys> m_keyStates{};
  std::array<bool, kMaxKeys> m_previousKeyStates{};

  // Mouse button state
  static constexpr size_t kMaxMouseButtons = 8;
  std::array<bool, kMaxMouseButtons> m_mouseButtonStates{};
  std::array<bool, kMaxMouseButtons> m_previousMouseButtonStates{};

  // Mouse position and deltas
  float m_mouseX{0.0f};
  float m_mouseY{0.0f};
  float m_mouseDeltaX{0.0f};
  float m_mouseDeltaY{0.0f};
  float m_scrollDeltaX{0.0f};
  float m_scrollDeltaY{0.0f};

  // Event queues
  std::queue<KeyEvent> m_keyEvents;
  std::queue<MouseButtonEvent> m_mouseButtonEvents;

  // Action bindings
  std::unordered_map<std::string, InputBinding> m_actionBindings;

  // Callbacks
  std::vector<KeyCallback> m_keyCallbacks;
  std::vector<MouseButtonCallback> m_mouseButtonCallbacks;
  std::vector<MouseMoveCallback> m_mouseMoveCallbacks;
  std::vector<MouseScrollCallback> m_mouseScrollCallbacks;
};

} // namespace Aetherion::Platform
