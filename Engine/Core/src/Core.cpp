#include "Aetherion/Core/Types.h"

#include <atomic>
#include <exception>
#include <iostream>
namespace Aetherion::Core
{
namespace
{
    std::atomic_bool g_coreInitialized{false};

    void InstallTerminateHandler() {
        std::set_terminate([]() {
            std::cerr << "[Aetherion] Fatal: std::terminate() called.\n";
            if (auto eptr = std::current_exception()) {
                try {
                    std::rethrow_exception(eptr);
                } catch (const std::exception& ex) {
                    std::cerr << "[Aetherion] Unhandled exception: " << ex.what() << "\n";
                } catch (...) {
                    std::cerr << "[Aetherion] Unhandled unknown exception.\n";
            }
        }
        std::abort();
        });
    }
}
void InitializeCoreModule()
{
    bool expected = false; // Idempotent initialization
    if (!g_coreInitialized.compare_exchange_strong(expected, true))
        return;

    InstallTerminateHandler();
    // TODO: Bootstrap shared services (logging, profiling, job system, configuration).
}

} // namespace Aetherion::Core
