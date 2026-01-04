#include <Aetherion/Assets/AssetRegistry.h>
#include <Aetherion/Scripting/ScriptingPlaceholder.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using Aetherion::Assets::AssetRegistry;
using Aetherion::Scripting::ScriptingRuntime;

namespace {

bool TestVirtualAssets() {
    std::cout << "[Test] Virtual asset registration\n";
    std::filesystem::path tempRoot =
        std::filesystem::temp_directory_path() / "aetherion_virtual_test";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);

    AssetRegistry registry;
    registry.Scan(tempRoot.string());

    registry.RegisterVirtualAsset("texture://generate/brick_wall",
                                  AssetRegistry::AssetType::Texture,
                                  tempRoot / "brick.virtual");

    const auto &virtuals = registry.GetVirtualAssets();
    if (virtuals.empty()) {
        std::cerr << "Virtual asset map empty\n";
        return false;
    }

    auto id = virtuals.begin()->first;
    if (!registry.HasAsset(id) || !registry.IsVirtualAsset(id)) {
        std::cerr << "Registry did not acknowledge virtual asset\n";
        return false;
    }

    const auto *entry = registry.FindEntry(id);
    if (!entry) {
        std::cerr << "Entry not found for virtual asset\n";
        return false;
    }
    if (!std::filesystem::exists(entry->path, ec)) {
        std::cerr << "Virtual asset cache path missing: " << entry->path
                  << "\n";
        return false;
    }

    std::filesystem::remove_all(tempRoot, ec);
    return true;
}

bool TestScriptingHotReload() {
    std::cout << "[Test] Scripting runtime hot-reload\n";
    std::filesystem::path tempRoot =
        std::filesystem::temp_directory_path() / "aetherion_script_test";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);

    const std::filesystem::path promptPath = tempRoot / "behavior.prompt.txt";
    {
        std::ofstream out(promptPath, std::ios::trunc);
        out << "Initial prompt text";
    }

    ScriptingRuntime runtime;
    runtime.SetOutputRoot(tempRoot / "out");
    runtime.Initialize();
    runtime.RegisterPromptAsset("testPrompt", promptPath);

    auto script = runtime.GetScript("testPrompt");
    if (!script || !std::filesystem::exists(script->generatedPath, ec)) {
        std::cerr << "Generated script missing\n";
        return false;
    }

    // Modify prompt file to trigger reload
    {
        std::ofstream out(promptPath, std::ios::trunc);
        out << "Updated prompt text";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    runtime.TickHotReload();

    auto updated = runtime.GetScript("testPrompt");
    if (!updated || !std::filesystem::exists(updated->generatedPath, ec)) {
        std::cerr << "Updated script missing\n";
        return false;
    }

    std::ifstream generated(updated->generatedPath);
    std::string contents((std::istreambuf_iterator<char>(generated)),
                         std::istreambuf_iterator<char>());
    if (contents.find("Updated prompt text") == std::string::npos) {
        std::cerr << "Hot reload did not regenerate script content\n";
        return false;
    }

    std::filesystem::remove_all(tempRoot, ec);
    return true;
}

} // namespace

int main() {
    int failures = 0;
    if (!TestVirtualAssets()) {
        ++failures;
    }
    if (!TestScriptingHotReload()) {
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All self-tests passed.\n";
    } else {
        std::cout << failures << " self-test(s) failed.\n";
    }
    return failures;
}
