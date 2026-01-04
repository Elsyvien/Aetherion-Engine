#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Aetherion/Runtime/RuntimeSystem.h"

namespace Aetherion::Runtime {

/// @brief System execution phase for deterministic ordering
enum class SystemPhase : uint8_t {
  PrePhysics = 0,  ///< Input, AI decisions, pre-simulation logic
  Physics = 1,     ///< Physics simulation step
  PostPhysics = 2, ///< Physics response, collision handling
  PreRender = 3,   ///< Transform updates, animation, LOD selection
  Render = 4,      ///< Rendering submission
  PostRender = 5,  ///< UI, debug overlays, cleanup
  Count = 6
};

/// @brief Priority within a phase (lower = earlier execution)
using SystemPriority = int32_t;

/// @brief System registration info with scheduling metadata
struct SystemInfo {
  std::shared_ptr<IRuntimeSystem> system;
  SystemPhase phase{SystemPhase::PrePhysics};
  SystemPriority priority{0};
  std::vector<std::string> dependencies; ///< Systems that must run before this
  bool enabled{true};
  bool initialized{false};
};

/// @brief Manages system execution order with dependency resolution
///
/// The SystemScheduler provides:
/// - Phase-based execution (PrePhysics -> Physics -> PostPhysics -> ...)
/// - Priority ordering within phases
/// - Explicit dependencies between systems
/// - Topological sorting with cycle detection
/// - Enable/disable individual systems at runtime
class SystemScheduler {
public:
  SystemScheduler() = default;
  ~SystemScheduler() = default;

  SystemScheduler(const SystemScheduler &) = delete;
  SystemScheduler &operator=(const SystemScheduler &) = delete;

  /// @brief Register a system with scheduling info
  /// @param system The system instance
  /// @param phase Execution phase
  /// @param priority Priority within phase (lower = earlier)
  /// @param dependencies Names of systems that must run before this one
  void RegisterSystem(std::shared_ptr<IRuntimeSystem> system,
                      SystemPhase phase = SystemPhase::PrePhysics,
                      SystemPriority priority = 0,
                      std::vector<std::string> dependencies = {});

  /// @brief Unregister a system by name
  void UnregisterSystem(const std::string &name);

  /// @brief Enable or disable a system
  void SetSystemEnabled(const std::string &name, bool enabled);

  /// @brief Check if a system is enabled
  [[nodiscard]] bool IsSystemEnabled(const std::string &name) const;

  /// @brief Get system info by name
  [[nodiscard]] const SystemInfo *GetSystemInfo(const std::string &name) const;

  /// @brief Build the execution order (call after all systems registered)
  /// @return true if order was built successfully, false if cycle detected
  bool BuildExecutionOrder();

  /// @brief Initialize all registered systems in order
  void InitializeSystems(EngineContext &context);

  /// @brief Tick all enabled systems in deterministic order
  void TickSystems(EngineContext &context, float deltaTime);

  /// @brief Tick only systems in a specific phase
  void TickPhase(EngineContext &context, float deltaTime, SystemPhase phase);

  /// @brief Shutdown all systems in reverse order
  void ShutdownSystems(EngineContext &context);

  /// @brief Get the current execution order (for debugging/inspection)
  [[nodiscard]] const std::vector<std::string> &GetExecutionOrder() const {
    return m_executionOrder;
  }

  /// @brief Get all registered system names
  [[nodiscard]] std::vector<std::string> GetRegisteredSystemNames() const;

  /// @brief Check if execution order is valid
  [[nodiscard]] bool IsOrderValid() const noexcept { return m_orderValid; }

  /// @brief Get diagnostic info about detected cycles (if any)
  [[nodiscard]] const std::string &GetCycleDiagnostic() const noexcept {
    return m_cycleDiagnostic;
  }

  /// @brief Get systems grouped by phase
  [[nodiscard]] std::vector<std::vector<std::string>> GetSystemsByPhase() const;

  /// @brief Callback type for system execution events
  using SystemCallback =
      std::function<void(const std::string &name, float durationMs)>;

  /// @brief Set callback for profiling system execution
  void SetProfileCallback(SystemCallback callback) {
    m_profileCallback = std::move(callback);
  }

private:
  struct DependencyNode {
    std::string name;
    SystemPhase phase;
    SystemPriority priority;
    std::vector<std::string> dependencies;
    bool enabled{true};
  };

  bool TopologicalSort();
  bool DetectCycle(const std::string &node,
                   std::unordered_set<std::string> &visiting,
                   std::unordered_set<std::string> &visited,
                   std::vector<std::string> &result);

  std::unordered_map<std::string, SystemInfo> m_systems;
  std::vector<std::string> m_executionOrder;
  bool m_orderValid{false};
  bool m_orderDirty{true};
  std::string m_cycleDiagnostic;
  SystemCallback m_profileCallback;
};

/// @brief Helper to get phase name for debugging
[[nodiscard]] inline constexpr const char *
GetPhaseName(SystemPhase phase) noexcept {
  switch (phase) {
  case SystemPhase::PrePhysics:
    return "PrePhysics";
  case SystemPhase::Physics:
    return "Physics";
  case SystemPhase::PostPhysics:
    return "PostPhysics";
  case SystemPhase::PreRender:
    return "PreRender";
  case SystemPhase::Render:
    return "Render";
  case SystemPhase::PostRender:
    return "PostRender";
  default:
    return "Unknown";
  }
}

} // namespace Aetherion::Runtime
