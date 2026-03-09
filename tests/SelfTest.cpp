#include <Aetherion/Assets/AssetRegistry.h>
#include <Aetherion/Assets/LatentAssetLoader.h>
#include <Aetherion/Scene/Entity.h>
#include <Aetherion/Scene/Scene.h>
#include <Aetherion/Scene/ScriptComponent.h>
#include <Aetherion/Scene/SemanticComponent.h>
#include <Aetherion/Scene/SemanticGraph.h>
#include <Aetherion/Scripting/ScriptInstance.h>
#include <Aetherion/Scripting/ScriptEngine.h>
#include <Aetherion/Scripting/ScriptingPlaceholder.h>
#include <Aetherion/Scripting/ScriptingSystem.h>
#include <Aetherion/Runtime/EngineContext.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using Aetherion::Assets::AssetRegistry;
using Aetherion::Assets::LatentAssetLoader;
using Aetherion::Assets::LatentDecoder;
using Aetherion::Runtime::EngineContext;
using Aetherion::Scripting::ScriptingRuntime;
using Aetherion::Scripting::ScriptingSystem;
using Aetherion::Scene::Scene;
using Aetherion::Scene::ScriptComponent;
using Aetherion::Scene::SemanticGraph;
using Aetherion::Scene::SemanticComponent;

namespace {

struct RecordingScriptInstance final : Aetherion::Scripting::ScriptInstance {
    int* destroyCount = nullptr;

    void SetEntity(Aetherion::Scene::Entity* entity) override {
        (void)entity;
    }

    void OnCreate() override {}
    void OnUpdate(float deltaTime) override { (void)deltaTime; }

    void OnDestroy() override {
        if (destroyCount) {
            ++(*destroyCount);
        }
    }
};

class RecordingScriptEngine final : public Aetherion::Scripting::ScriptEngine {
public:
    void Initialize() override {}
    void Shutdown() override {}

    std::unique_ptr<Aetherion::Scripting::ScriptInstance>
    CreateInstance(const std::string& scriptSource,
                   SourceKind sourceKind) override {
        createdSources.push_back(scriptSource);
        createdKinds.push_back(sourceKind);
        auto instance = std::make_unique<RecordingScriptInstance>();
        instance->destroyCount = &destroyedInstances;
        return instance;
    }

    void OnUpdate(float deltaTime) override {
        (void)deltaTime;
        ++updateCalls;
    }

    std::vector<std::string> createdSources;
    std::vector<SourceKind> createdKinds;
    int destroyedInstances = 0;
    int updateCalls = 0;
};

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

bool TestSceneScriptingSystemResolvesAssetScripts() {
    std::cout << "[Test] Scene scripting resolves asset-backed Lua scripts\n";

    std::filesystem::path tempRoot =
        std::filesystem::temp_directory_path() / "aetherion_scene_script_test";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot / "scripts", ec);

    const std::filesystem::path scriptPath = tempRoot / "scripts" / "spin.lua";
    {
        std::ofstream out(scriptPath, std::ios::trunc);
        out << "return { on_update = function(self, dt) end }\n";
    }

    AssetRegistry registry;
    registry.Scan(tempRoot.string());

    const AssetRegistry::AssetEntry* scriptEntry = nullptr;
    for (const auto& entry : registry.GetEntries()) {
        if (entry.type == AssetRegistry::AssetType::Script &&
            entry.path.filename() == scriptPath.filename()) {
            scriptEntry = &entry;
            break;
        }
    }
    if (!scriptEntry) {
        std::cerr << "Failed to register Lua script asset\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    EngineContext context;
    context.SetAssetRegistry(std::make_shared<AssetRegistry>(registry));

    Scene scene("ScriptScene");
    scene.BindContext(context);

    auto entity = scene.CreateEntity("Spinner");
    auto scriptComponent = entity->AddComponent<ScriptComponent>();
    scriptComponent->SetSourceMode(ScriptComponent::SourceMode::FileReference);
    scriptComponent->SetScriptAssetId(scriptEntry->id);

    RecordingScriptEngine engine;
    ScriptingSystem scriptingSystem(&engine);
    scriptingSystem.BindScene(&scene);
    scriptingSystem.Update(0.016f);

    if (engine.createdSources.size() != 1) {
        std::cerr << "Expected one script instance creation\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    const std::filesystem::path createdPath(engine.createdSources.front());
    if (createdPath.filename() != scriptPath.filename()) {
        std::cerr << "Script asset ID did not resolve to source path\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }
    if (engine.createdKinds.front() != RecordingScriptEngine::SourceKind::FilePath) {
        std::cerr << "Expected file-backed asset script to use file source kind\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::ofstream out(scriptPath, std::ios::trunc);
        out << "return { on_update = function(self, dt) print(dt) end }\n";
    }

    scriptingSystem.Update(0.016f);

    if (engine.createdSources.size() != 2) {
        std::cerr << "Expected file-backed script hot reload to recreate instance\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    if (engine.destroyedInstances < 1) {
        std::cerr << "Expected previous script instance to be destroyed on reload\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    scriptingSystem.UnbindScene();
    std::filesystem::remove_all(tempRoot, ec);
    return true;
}

bool TestSceneScriptingSystemSupportsLegacyAssetIdScripts() {
    std::cout << "[Test] Scene scripting supports legacy assetId-only scripts\n";

    std::filesystem::path tempRoot =
        std::filesystem::temp_directory_path() / "aetherion_scene_script_legacy_test";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot / "scripts", ec);

    const std::filesystem::path scriptPath = tempRoot / "scripts" / "legacy.lua";
    {
        std::ofstream out(scriptPath, std::ios::trunc);
        out << "return { on_update = function(self, dt) end }\n";
    }

    AssetRegistry registry;
    registry.Scan(tempRoot.string());

    const AssetRegistry::AssetEntry* scriptEntry = nullptr;
    for (const auto& entry : registry.GetEntries()) {
        if (entry.type == AssetRegistry::AssetType::Script &&
            entry.path.filename() == scriptPath.filename()) {
            scriptEntry = &entry;
            break;
        }
    }
    if (!scriptEntry) {
        std::cerr << "Failed to register legacy Lua script asset\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    EngineContext context;
    context.SetAssetRegistry(std::make_shared<AssetRegistry>(registry));

    Scene scene("LegacyScriptScene");
    scene.BindContext(context);

    auto entity = scene.CreateEntity("LegacySpinner");
    auto scriptComponent = entity->AddComponent<ScriptComponent>();
    scriptComponent->SetScriptAssetId(scriptEntry->id);

    RecordingScriptEngine engine;
    ScriptingSystem scriptingSystem(&engine);
    scriptingSystem.BindScene(&scene);
    scriptingSystem.Update(0.016f);

    if (engine.createdKinds.size() != 1 ||
        engine.createdKinds.front() != RecordingScriptEngine::SourceKind::FilePath) {
        std::cerr << "Legacy assetId-only script did not resolve as file-backed\n";
        std::filesystem::remove_all(tempRoot, ec);
        return false;
    }

    scriptingSystem.UnbindScene();
    std::filesystem::remove_all(tempRoot, ec);
    return true;
}

bool TestSceneScriptingSystemKeepsInlineLuaInline() {
    std::cout << "[Test] Scene scripting preserves inline Lua source mode\n";

    Scene scene("InlineScene");
    auto entity = scene.CreateEntity("InlineScriptEntity");
    auto scriptComponent = entity->AddComponent<ScriptComponent>();
    scriptComponent->SetScriptSource(
        "local note = '.lua embedded in code'\n"
        "return { on_update = function(self, dt) end }\n");

    RecordingScriptEngine engine;
    ScriptingSystem scriptingSystem(&engine);
    scriptingSystem.BindScene(&scene);
    scriptingSystem.Update(0.016f);

    if (engine.createdKinds.size() != 1 ||
        engine.createdKinds.front() != RecordingScriptEngine::SourceKind::InlineCode) {
        std::cerr << "Inline Lua was misclassified as a file source\n";
        return false;
    }

    scriptingSystem.UnbindScene();
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
    if (!TestSceneScriptingSystemResolvesAssetScripts()) {
        ++failures;
    }
    if (!TestSceneScriptingSystemSupportsLegacyAssetIdScripts()) {
        ++failures;
    }
    if (!TestSceneScriptingSystemKeepsInlineLuaInline()) {
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All self-tests passed.\n";
    } else {
        std::cout << failures << " self-test(s) failed.\n";
    }
    return failures;
}
