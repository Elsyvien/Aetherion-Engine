#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aetherion::Core {

/// @brief High-resolution timer for performance measurement
class Timer {
public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::duration<double>;

  Timer() : m_startTime(Clock::now()) {}

  /// @brief Start/restart the timer
  void Start() noexcept { m_startTime = Clock::now(); }

  /// @brief Get elapsed time in seconds
  [[nodiscard]] double ElapsedSeconds() const noexcept {
    return Duration(Clock::now() - m_startTime).count();
  }

  /// @brief Get elapsed time in milliseconds
  [[nodiscard]] double ElapsedMilliseconds() const noexcept {
    return ElapsedSeconds() * 1000.0;
  }

  /// @brief Get elapsed time in microseconds
  [[nodiscard]] double ElapsedMicroseconds() const noexcept {
    return ElapsedSeconds() * 1000000.0;
  }

  /// @brief Restart and return elapsed time in seconds
  [[nodiscard]] double Restart() noexcept {
    const auto now = Clock::now();
    const double elapsed = Duration(now - m_startTime).count();
    m_startTime = now;
    return elapsed;
  }

private:
  TimePoint m_startTime;
};

/// @brief Statistics for a profiled scope
struct ProfileStats {
  std::string name;
  double lastMs{0.0};
  double minMs{std::numeric_limits<double>::max()};
  double maxMs{0.0};
  double avgMs{0.0};
  double totalMs{0.0};
  uint64_t callCount{0};

  void AddSample(double ms) noexcept {
    lastMs = ms;
    minMs = std::min(minMs, ms);
    maxMs = std::max(maxMs, ms);
    totalMs += ms;
    ++callCount;
    avgMs = totalMs / static_cast<double>(callCount);
  }

  void Reset() noexcept {
    lastMs = 0.0;
    minMs = std::numeric_limits<double>::max();
    maxMs = 0.0;
    avgMs = 0.0;
    totalMs = 0.0;
    callCount = 0;
  }
};

/// @brief Frame timing statistics
struct FrameStats {
  double deltaTimeMs{0.0};
  double fps{0.0};
  double avgFps{0.0};
  double minFps{std::numeric_limits<double>::max()};
  double maxFps{0.0};
  uint64_t frameCount{0};
  double totalTimeMs{0.0};

  void AddFrame(double dtMs) noexcept {
    deltaTimeMs = dtMs;
    fps = dtMs > 0.0 ? 1000.0 / dtMs : 0.0;
    minFps = std::min(minFps, fps);
    maxFps = std::max(maxFps, fps);
    totalTimeMs += dtMs;
    ++frameCount;
    avgFps = frameCount > 0 ? (frameCount * 1000.0 / totalTimeMs) : 0.0;
  }

  void Reset() noexcept {
    deltaTimeMs = 0.0;
    fps = 0.0;
    avgFps = 0.0;
    minFps = std::numeric_limits<double>::max();
    maxFps = 0.0;
    frameCount = 0;
    totalTimeMs = 0.0;
  }
};

/// @brief Thread-safe profiler for measuring code performance
///
/// Usage:
/// @code
/// // Manual timing
/// Profiler::Get().BeginScope("Physics");
/// // ... physics code ...
/// Profiler::Get().EndScope("Physics");
///
/// // RAII timing
/// {
///   AETHERION_PROFILE_SCOPE("Rendering");
///   // ... render code ...
/// }
///
/// // Frame timing
/// Profiler::Get().BeginFrame();
/// // ... frame code ...
/// Profiler::Get().EndFrame();
/// @endcode
class Profiler {
public:
  /// @brief Get the singleton instance
  static Profiler &Get() {
    static Profiler instance;
    return instance;
  }

  /// @brief Enable or disable profiling
  void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
  [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

  /// @brief Begin a named profiling scope
  void BeginScope(const std::string &name) {
    if (!m_enabled)
      return;

    std::lock_guard lock(m_mutex);
    m_activeScopes[name] = Timer();
  }

  /// @brief End a named profiling scope
  void EndScope(const std::string &name) {
    if (!m_enabled)
      return;

    std::lock_guard lock(m_mutex);
    auto it = m_activeScopes.find(name);
    if (it != m_activeScopes.end()) {
      const double ms = it->second.ElapsedMilliseconds();
      m_stats[name].name = name;
      m_stats[name].AddSample(ms);
      m_activeScopes.erase(it);
    }
  }

  /// @brief Record a scope with explicit duration
  void RecordScope(const std::string &name, double ms) {
    if (!m_enabled)
      return;

    std::lock_guard lock(m_mutex);
    m_stats[name].name = name;
    m_stats[name].AddSample(ms);
  }

  /// @brief Begin frame timing
  void BeginFrame() {
    if (!m_enabled)
      return;

    m_frameTimer.Start();
  }

  /// @brief End frame timing
  void EndFrame() {
    if (!m_enabled)
      return;

    const double dtMs = m_frameTimer.ElapsedMilliseconds();
    std::lock_guard lock(m_mutex);
    m_frameStats.AddFrame(dtMs);
  }

  /// @brief Get stats for a specific scope
  [[nodiscard]] ProfileStats GetScopeStats(const std::string &name) const {
    std::lock_guard lock(m_mutex);
    auto it = m_stats.find(name);
    return it != m_stats.end() ? it->second : ProfileStats{name};
  }

  /// @brief Get all scope stats
  [[nodiscard]] std::vector<ProfileStats> GetAllStats() const {
    std::lock_guard lock(m_mutex);
    std::vector<ProfileStats> result;
    result.reserve(m_stats.size());
    for (const auto &[name, stats] : m_stats) {
      result.push_back(stats);
    }
    // Sort by total time descending (hottest first)
    std::sort(result.begin(), result.end(),
              [](const ProfileStats &a, const ProfileStats &b) {
                return a.totalMs > b.totalMs;
              });
    return result;
  }

  /// @brief Get frame statistics
  [[nodiscard]] FrameStats GetFrameStats() const {
    std::lock_guard lock(m_mutex);
    return m_frameStats;
  }

  /// @brief Reset all statistics
  void Reset() {
    std::lock_guard lock(m_mutex);
    m_stats.clear();
    m_frameStats.Reset();
  }

  /// @brief Reset statistics for a specific scope
  void ResetScope(const std::string &name) {
    std::lock_guard lock(m_mutex);
    auto it = m_stats.find(name);
    if (it != m_stats.end()) {
      it->second.Reset();
    }
  }

  /// @brief Get formatted report string
  [[nodiscard]] std::string GetReport() const {
    std::lock_guard lock(m_mutex);

    std::string report = "=== Profiler Report ===\n";
    report += "Frame: " + std::to_string(m_frameStats.frameCount) +
              " | FPS: " + std::to_string(static_cast<int>(m_frameStats.fps)) +
              " (avg: " +
              std::to_string(static_cast<int>(m_frameStats.avgFps)) + ")\n";
    report += "---\n";

    std::vector<ProfileStats> sorted;
    for (const auto &[name, stats] : m_stats) {
      sorted.push_back(stats);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const ProfileStats &a, const ProfileStats &b) {
                return a.totalMs > b.totalMs;
              });

    for (const auto &stats : sorted) {
      report += stats.name + ": " +
                std::to_string(static_cast<int>(stats.lastMs * 100) / 100.0) +
                " ms (avg: " +
                std::to_string(static_cast<int>(stats.avgMs * 100) / 100.0) +
                " ms, calls: " + std::to_string(stats.callCount) + ")\n";
    }

    return report;
  }

private:
  Profiler() = default;
  ~Profiler() = default;

  Profiler(const Profiler &) = delete;
  Profiler &operator=(const Profiler &) = delete;

  mutable std::mutex m_mutex;
  std::unordered_map<std::string, Timer> m_activeScopes;
  std::unordered_map<std::string, ProfileStats> m_stats;
  FrameStats m_frameStats;
  Timer m_frameTimer;
  bool m_enabled{true};
};

/// @brief RAII scope profiler
class ScopedProfile {
public:
  explicit ScopedProfile(const char *name) : m_name(name) {
    Profiler::Get().BeginScope(m_name);
  }

  ~ScopedProfile() { Profiler::Get().EndScope(m_name); }

  ScopedProfile(const ScopedProfile &) = delete;
  ScopedProfile &operator=(const ScopedProfile &) = delete;

private:
  const char *m_name;
};

/// @brief Rolling average calculator for smooth statistics
template <size_t N = 60> class RollingAverage {
public:
  void AddSample(double value) noexcept {
    m_samples[m_index] = value;
    m_index = (m_index + 1) % N;
    if (m_count < N) {
      ++m_count;
    }
  }

  [[nodiscard]] double GetAverage() const noexcept {
    if (m_count == 0)
      return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < m_count; ++i) {
      sum += m_samples[i];
    }
    return sum / static_cast<double>(m_count);
  }

  [[nodiscard]] double GetMin() const noexcept {
    if (m_count == 0)
      return 0.0;
    double minVal = m_samples[0];
    for (size_t i = 1; i < m_count; ++i) {
      minVal = std::min(minVal, m_samples[i]);
    }
    return minVal;
  }

  [[nodiscard]] double GetMax() const noexcept {
    if (m_count == 0)
      return 0.0;
    double maxVal = m_samples[0];
    for (size_t i = 1; i < m_count; ++i) {
      maxVal = std::max(maxVal, m_samples[i]);
    }
    return maxVal;
  }

  [[nodiscard]] double GetLast() const noexcept {
    if (m_count == 0)
      return 0.0;
    return m_samples[(m_index + N - 1) % N];
  }

  void Reset() noexcept {
    m_index = 0;
    m_count = 0;
  }

private:
  std::array<double, N> m_samples{};
  size_t m_index{0};
  size_t m_count{0};
};

} // namespace Aetherion::Core

// ============================================================================
// Profiling Macros
// ============================================================================

#ifdef AETHERION_ENABLE_PROFILING
#define AETHERION_PROFILE_SCOPE(name)                                          \
  ::Aetherion::Core::ScopedProfile _aetherion_profile_##__LINE__(name)
#define AETHERION_PROFILE_FUNCTION()                                           \
  AETHERION_PROFILE_SCOPE(__FUNCTION__)
#define AETHERION_PROFILE_BEGIN(name)                                          \
  ::Aetherion::Core::Profiler::Get().BeginScope(name)
#define AETHERION_PROFILE_END(name)                                            \
  ::Aetherion::Core::Profiler::Get().EndScope(name)
#define AETHERION_PROFILE_FRAME_BEGIN()                                        \
  ::Aetherion::Core::Profiler::Get().BeginFrame()
#define AETHERION_PROFILE_FRAME_END()                                          \
  ::Aetherion::Core::Profiler::Get().EndFrame()
#else
#define AETHERION_PROFILE_SCOPE(name) ((void)0)
#define AETHERION_PROFILE_FUNCTION() ((void)0)
#define AETHERION_PROFILE_BEGIN(name) ((void)0)
#define AETHERION_PROFILE_END(name) ((void)0)
#define AETHERION_PROFILE_FRAME_BEGIN() ((void)0)
#define AETHERION_PROFILE_FRAME_END() ((void)0)
#endif
