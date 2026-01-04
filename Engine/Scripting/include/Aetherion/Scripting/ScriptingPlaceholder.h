#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef AETHERION_ENABLE_PYTHON
#include <Python.h>
#endif
namespace Aetherion::Scripting {

enum class BehaviorExecutionMode { Stub, LocalModel, RemoteService };

struct BehaviorPrompt {
    std::string id;
    std::string name;
    std::string promptText;
    std::filesystem::path sourcePath;
    std::uint64_t version{0};
    std::filesystem::file_time_type lastWriteTime{};
};

struct BehaviorScript {
    std::string id;
    std::string code;
    std::uint64_t version{0};
    std::filesystem::path generatedPath;
    std::string diagnostics;
};

struct BehaviorDecision {
    std::string state{"Idle"};
    std::string reason{"Uninitialized"};
    bool success{true};
};

// Responsible for turning prompts into executable Python snippets.
class PromptToScriptGenerator {
public:
    PromptToScriptGenerator() = default;
    BehaviorScript Generate(const BehaviorPrompt& prompt,
                            const std::filesystem::path& outputRoot) const;
};

// Minimal scripting runtime that ingests prompt assets, generates Python
// scripts, and supports hot reload via file timestamps. This keeps the surface
// area small so the editor/runtime can integrate while a real Python/LLM
// backend is wired in later.
class ScriptingRuntime {
public:
    ScriptingRuntime();
    ~ScriptingRuntime();

    using ErrorSink = std::function<void(const std::string&)>;

    void Initialize();
    void Shutdown();

    void SetErrorSink(ErrorSink sink) { m_errorSink = std::move(sink); }
    void EnablePythonBridge(bool enabled) noexcept { m_pythonEnabled = enabled; }

    void SetOutputRoot(std::filesystem::path root);
    [[nodiscard]] const std::filesystem::path& GetOutputRoot() const noexcept {
        return m_outputRoot;
    }

    // Register a prompt that lives on disk (plain text or JSON with "prompt").
    BehaviorScript RegisterPromptAsset(const std::string& assetId,
                                       const std::filesystem::path& promptPath);
    // Register an inline prompt (useful for unit tests or editor prototyping).
    BehaviorScript RegisterInlinePrompt(const std::string& assetId,
                                        const std::string& promptText,
                                        const std::string& nameHint = {});

    void UpdatePromptText(const std::string& assetId,
                          const std::string& promptText);
    void TickHotReload();

    [[nodiscard]] std::optional<BehaviorScript>
    GetScript(const std::string& assetId) const;
    [[nodiscard]] BehaviorDecision RunBehavior(const std::string& assetId,
                                               const std::string& contextJson);
    [[nodiscard]] bool HasPrompt(const std::string& assetId) const noexcept;

private:
    void InitializePython();
    BehaviorDecision RunBehaviorPython(const BehaviorScript& script,
                                       const std::string& contextJson);
    BehaviorPrompt LoadPromptFromDisk(const std::string& assetId,
                                      const std::filesystem::path& path) const;
    BehaviorScript GenerateScript(const BehaviorPrompt& prompt);
    void EnsureOutputDir();
    [[nodiscard]] std::filesystem::path
    GetScriptPath(const BehaviorPrompt& prompt) const;

    bool m_initialized{false};
    bool m_pythonEnabled{false};
    bool m_pythonInitialized{false};
    std::filesystem::path m_outputRoot;
    std::unordered_map<std::string, BehaviorPrompt> m_prompts;
    std::unordered_map<std::string, BehaviorScript> m_scripts;
    PromptToScriptGenerator m_generator;
    ErrorSink m_errorSink;
};

// Backward-compatible alias while the rest of the engine migrates from the
// stubbed runtime name.
using ScriptingRuntimeStub = ScriptingRuntime;

inline void TouchScriptingModule() {}
} // namespace Aetherion::Scripting
