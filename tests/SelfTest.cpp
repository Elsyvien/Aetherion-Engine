#include <Aetherion/Assets/AssetRegistry.h>
#include <Aetherion/Assets/LatentAssetLoader.h>
#include <Aetherion/Scripting/ScriptingPlaceholder.h>
#include <Aetherion/Scene/Scene.h>
#include <Aetherion/Scene/Entity.h>
#include <Aetherion/Scene/SemanticComponent.h>
#include <Aetherion/Scene/SemanticGraph.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using Aetherion::Assets::AssetRegistry;
using Aetherion::Assets::LatentAssetLoader;
using Aetherion::Assets::LatentDecoder;
using Aetherion::Scripting::ScriptingRuntime;
using Aetherion::Scene::Scene;
using Aetherion::Scene::SemanticGraph;
using Aetherion::Scene::SemanticComponent;

namespace {

bool TestSemanticGraph() {
    std::cout << "[Test] Semantic Graph\n";
    Scene scene("TestScene");
    
    auto entity = scene.CreateEntity("Chair");
    auto semantic = std::make_unique<SemanticComponent>();
    semantic->AddTag("Furniture");
    semantic->AddTag("Wood");
    semantic->SetDescription("A sturdy wooden chair.");
    semantic->SetAttribute("Flammability", 0.8f);
    
    // Manually adding component for test since Entity API might be restricted or require shared_ptr
    // Assuming Entity has AddComponent method taking unique_ptr or shared_ptr
    // Checking Entity.h... CreateEntity returns shared_ptr.
    // Entity::AddComponent<T>(args...) is typical.
    entity->AddComponent<SemanticComponent>(); 
    auto comp = entity->GetComponent<SemanticComponent>();
    comp->AddTag("Furniture");
    comp->AddTag("Wood");
    comp->SetDescription("A sturdy wooden chair.");
    comp->SetAttribute("Flammability", 0.8f);

    SemanticGraph graph(&scene);
    auto furniture = graph.FindEntitiesWithTag("Furniture");
    if (furniture.empty()) {
        std::cerr << "Failed to find entity by tag 'Furniture'\n";
        return false;
    }

    auto relevant = graph.FindContextuallyRelevantEntities("wooden");
    if (relevant.empty()) {
        std::cerr << "Failed to find contextually relevant entity 'wooden'\n";
        return false;
    }
    
    return true;
}

bool TestLatentAssets() {
    std::cout << "[Test] Latent Assets\n";
    
    Aetherion::Assets::LatentAsset asset;
    asset.data.resize(10, 0.5f);
    asset.modelId = "test-model";
    
    int w, h;
    auto image = LatentDecoder::DecodeToImage(asset, w, h);
    
    if (image.empty() || w <= 0 || h <= 0) {
        std::cerr << "Failed to decode latent asset to image\n";
        return false;
    }
    
    return true;
}

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
    if (!TestSemanticGraph()) {
        ++failures;
    }
    if (!TestLatentAssets()) {
        ++failures;
    }
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
