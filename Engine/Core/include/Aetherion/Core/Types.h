#pragma once

#include <cstdint>
#include <string>
#include <format>
#include <compare>

#include <functional>
#include <vector>

namespace Aetherion::Core
{
struct Version
{
    int major = 0;
    int minor = 1;
    int patch = 0;

    [[nodiscard]] std::string ToString() const { return std::format("{}.{}.{}", major, minor, patch); }

    auto operator<=>(const Version&) const = default;

    // TODO: Extend with metadata (build hash, channel, etc.)
};

using EntityId = std::uint64_t;
using ComponentId = std::uint64_t;

class NonCopyable
{
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

struct EnginePaths
{
    std::string root;
    std::string content;
    std::string cache;

    // TODO: Integrate path resolution and virtual file systems.
};

enum class LogLevel { Info, Warning, Error, Debug };

class Log {
public:
    using LogCallback = std::function<void(LogLevel, const std::string&)>;

    static void Print(LogLevel level, const std::string& message);
    static void Info(const std::string& message) { Print(LogLevel::Info, message); }
    static void Warning(const std::string& message) { Print(LogLevel::Warning, message); }
    static void Error(const std::string& message) { Print(LogLevel::Error, message); }
    static void Debug(const std::string& message) { Print(LogLevel::Debug, message); }

    static void AddListener(LogCallback callback);

private:
    static std::vector<LogCallback> s_listeners;
};

void InitializeCoreModule();
// TODO: Wire up diagnostics, logging, timekeeping, and configuration plumbing.
} // namespace Aetherion::Core
