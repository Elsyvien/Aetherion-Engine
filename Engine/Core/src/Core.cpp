#include "Aetherion/Core/Types.h"
#include <iostream>

namespace Aetherion::Core
{
std::vector<Log::LogCallback> Log::s_listeners;

void InitializeCoreModule()
{
    // Placeholder for core subsystem initialization.
}

void Log::Print(LogLevel level, const std::string& message) {
    switch (level) {
        case LogLevel::Info: std::cout << "[INFO] " << message << std::endl; break;
        case LogLevel::Warning: std::cout << "[WARN] " << message << std::endl; break;
        case LogLevel::Error: std::cerr << "[ERROR] " << message << std::endl; break;
        case LogLevel::Debug: std::cout << "[DEBUG] " << message << std::endl; break;
    }

    for (const auto& callback : s_listeners) {
        callback(level, message);
    }
}

void Log::AddListener(LogCallback callback) {
    s_listeners.push_back(std::move(callback));
}
} // namespace Aetherion::Core
