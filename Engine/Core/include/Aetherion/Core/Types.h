#pragma once

#include <cstdint>
#include <string>

namespace Aetherion::Core
{
struct Version
{
    int major = 0;
    int minor = 1;
    int patch = 0;

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

void InitializeCoreModule();
// TODO: Wire up diagnostics, logging, timekeeping, and configuration plumbing.
} // namespace Aetherion::Core
