#include "Aetherion/Scripting/ScriptingPlaceholder.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>

#include "nlohmann/json.hpp"

namespace {
std::string EscapeForTripleQuote(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        if (c == '\'') {
            escaped += "\\'";
        } else {
            escaped.push_back(c);
        }
    }
    return escaped;
}

std::string SanitizeFileStem(const std::string& name) {
    std::string sanitized;
    sanitized.reserve(name.size());
    for (char c : name) {
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9');
        sanitized.push_back(alnum ? c : '_');
    }
    if (sanitized.empty()) {
        sanitized = "behavior";
    }
    return sanitized;
}
} // namespace

namespace Aetherion::Scripting {

ScriptingRuntime::ScriptingRuntime() = default;
ScriptingRuntime::~ScriptingRuntime() = default;

void ScriptingRuntime::Initialize() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    EnsureOutputDir();
}

void ScriptingRuntime::Shutdown() {
    m_initialized = false;
    m_prompts.clear();
    m_scripts.clear();
}

void ScriptingRuntime::SetOutputRoot(std::filesystem::path root) {
    m_outputRoot = std::move(root);
    if (m_initialized) {
        EnsureOutputDir();
    }
}

BehaviorPrompt ScriptingRuntime::LoadPromptFromDisk(
    const std::string& assetId, const std::filesystem::path& path) const {
    BehaviorPrompt prompt{};
    prompt.id = assetId;
    prompt.name = path.stem().string();
    prompt.sourcePath = path;

    std::ifstream input(path);
    if (input.is_open()) {
        std::stringstream buffer;
        buffer << input.rdbuf();
        const std::string contents = buffer.str();

        // Try JSON first; fall back to treating the file as plain text.
        bool parsedJson = false;
        try {
            auto root = nlohmann::json::parse(contents);
            if (root.is_object() && root.contains("prompt") &&
                root["prompt"].is_string()) {
                prompt.promptText = root["prompt"].get<std::string>();
                parsedJson = true;
            }
        } catch (...) {
        }
        if (!parsedJson) {
            prompt.promptText = contents;
        }
    } else {
        prompt.promptText = "Behavior prompt missing on disk.";
    }

    std::error_code ec;
    prompt.lastWriteTime = std::filesystem::last_write_time(path, ec);
    return prompt;
}

BehaviorScript ScriptingRuntime::GenerateScript(const BehaviorPrompt& prompt) {
    if (!m_initialized) {
        Initialize();
    }

    BehaviorScript script = m_generator.Generate(prompt, m_outputRoot);
    m_scripts[prompt.id] = script;
    return script;
}

BehaviorScript ScriptingRuntime::RegisterPromptAsset(
    const std::string& assetId, const std::filesystem::path& promptPath) {
    BehaviorPrompt prompt = LoadPromptFromDisk(assetId, promptPath);
    prompt.version += 1;
    m_prompts[assetId] = prompt;
    return GenerateScript(prompt);
}

BehaviorScript ScriptingRuntime::RegisterInlinePrompt(
    const std::string& assetId, const std::string& promptText,
    const std::string& nameHint) {
    BehaviorPrompt prompt{};
    prompt.id = assetId;
    prompt.name = nameHint.empty() ? assetId : nameHint;
    prompt.promptText = promptText;
    prompt.version += 1;
    m_prompts[assetId] = prompt;
    return GenerateScript(prompt);
}

void ScriptingRuntime::UpdatePromptText(const std::string& assetId,
                                        const std::string& promptText) {
    auto it = m_prompts.find(assetId);
    if (it == m_prompts.end()) {
        RegisterInlinePrompt(assetId, promptText, assetId);
        return;
    }

    it->second.promptText = promptText;
    it->second.version += 1;
    GenerateScript(it->second);
}

void ScriptingRuntime::TickHotReload() {
    for (auto& [id, prompt] : m_prompts) {
        if (prompt.sourcePath.empty()) {
            continue;
        }
        std::error_code ec;
        const auto writeTime = std::filesystem::last_write_time(
            prompt.sourcePath, ec);
        if (ec || writeTime == prompt.lastWriteTime) {
            continue;
        }

        prompt = LoadPromptFromDisk(id, prompt.sourcePath);
        prompt.version += 1;
        m_prompts[id] = prompt;
        GenerateScript(prompt);
    }
}

std::optional<BehaviorScript>
ScriptingRuntime::GetScript(const std::string& assetId) const {
    auto it = m_scripts.find(assetId);
    if (it == m_scripts.end()) {
        return std::nullopt;
    }
    return it->second;
}

BehaviorDecision ScriptingRuntime::RunBehavior(const std::string& assetId,
                                               const std::string& contextJson) {
    BehaviorDecision decision{};
    decision.reason = "Executed stub behavior";

    if (!m_pythonEnabled) {
        decision.reason += " (python bridge disabled)";
    }

    auto it = m_scripts.find(assetId);
    if (it == m_scripts.end()) {
        decision.success = false;
        decision.reason = "Script not found for asset id";
        if (m_errorSink) {
            m_errorSink("[ScriptingRuntime] Missing script for asset: " + assetId);
        }
        return decision;
    }

    if (m_pythonEnabled) {
        return RunBehaviorPython(it->second, contextJson);
    }

    // Stub: Inspect context for simple state hints
    if (contextJson.find("attack") != std::string::npos) {
        decision.state = "Attack";
        decision.reason = "Context mentioned attack";
    } else if (contextJson.find("patrol") != std::string::npos) {
        decision.state = "Patrol";
        decision.reason = "Context mentioned patrol";
    } else {
        decision.state = "Idle";
    }
    return decision;
}

void ScriptingRuntime::InitializePython() {
#ifdef AETHERION_ENABLE_PYTHON
    static std::once_flag once;
    std::call_once(once, []() { Py_Initialize(); });
    m_pythonInitialized = Py_IsInitialized();
#else
    m_pythonInitialized = false;
#endif
}

BehaviorDecision ScriptingRuntime::RunBehaviorPython(
    const BehaviorScript& script, const std::string& contextJson) {
    BehaviorDecision decision{};
    decision.reason = "Python bridge disabled";

#ifdef AETHERION_ENABLE_PYTHON
    if (!m_pythonInitialized) {
        InitializePython();
    }
    if (!m_pythonInitialized) {
        decision.success = false;
        decision.reason = "Python initialization failed";
        return decision;
    }

    PyGILState_STATE gil = PyGILState_Ensure();
    PyObject* globals = PyDict_New();
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());

    std::string code = script.code;
    if (!script.generatedPath.empty()) {
        std::ifstream input(script.generatedPath);
        if (input.is_open()) {
            std::stringstream buffer;
            buffer << input.rdbuf();
            code = buffer.str();
        }
    }

    decision.state = "Idle";
    decision.reason = "Python execution ok";

    PyObject* runResult =
        PyRun_StringFlags(code.c_str(), Py_file_input, globals, globals, nullptr);
    if (!runResult) {
        if (m_errorSink) {
            m_errorSink("[ScriptingRuntime] Python execution failed");
        }
        PyErr_Print();
        decision.success = false;
        decision.reason = "Python execution failed";
        Py_DECREF(globals);
        PyGILState_Release(gil);
        return decision;
    }
    Py_DECREF(runResult);

    PyObject* updateFunc = PyDict_GetItemString(globals, "update");
    if (updateFunc && PyCallable_Check(updateFunc)) {
        PyObject* ctxDict = PyDict_New();
        PyDict_SetItemString(ctxDict, "context", PyUnicode_FromString(contextJson.c_str()));
        PyObject* result =
            PyObject_CallFunctionObjArgs(updateFunc, Py_None, ctxDict, nullptr);
        Py_DECREF(ctxDict);
        if (result && PyDict_Check(result)) {
            PyObject* stateObj = PyDict_GetItemString(result, "state");
            PyObject* reasonObj = PyDict_GetItemString(result, "reason");
            if (stateObj && PyUnicode_Check(stateObj)) {
                decision.state = PyUnicode_AsUTF8(stateObj);
            }
            if (reasonObj && PyUnicode_Check(reasonObj)) {
                decision.reason = PyUnicode_AsUTF8(reasonObj);
            }
            Py_DECREF(result);
        } else {
            decision.success = false;
            decision.reason = "update() did not return a dict";
            if (m_errorSink) {
                m_errorSink("[ScriptingRuntime] update() did not return a dict");
            }
            if (result) {
                Py_DECREF(result);
            }
        }
    } else {
        decision.success = false;
        decision.reason = "No update() function found in script";
        if (m_errorSink) {
            m_errorSink("[ScriptingRuntime] No update() in behavior script");
        }
    }

    Py_DECREF(globals);
    PyGILState_Release(gil);
    return decision;
#else
    (void)script;
    (void)contextJson;
    decision.success = false;
    decision.reason = "Python bridge not compiled";
    return decision;
#endif
}

bool ScriptingRuntime::HasPrompt(const std::string& assetId) const noexcept {
    return m_prompts.find(assetId) != m_prompts.end();
}

void ScriptingRuntime::EnsureOutputDir() {
    if (m_outputRoot.empty()) {
        std::error_code ec;
        m_outputRoot =
            std::filesystem::current_path(ec) / "build" / "scripting_cache";
    }

    std::error_code ec;
    std::filesystem::create_directories(m_outputRoot, ec);
    if (ec) {
        std::cerr << "[ScriptingRuntime] Failed to create output directory: "
                  << m_outputRoot << "\n";
    }
}

std::filesystem::path
ScriptingRuntime::GetScriptPath(const BehaviorPrompt& prompt) const {
    if (m_outputRoot.empty()) {
        return {};
    }
    return m_outputRoot / (SanitizeFileStem(prompt.id.empty()
                                                ? prompt.name
                                                : prompt.id) +
                           ".py");
}

BehaviorScript PromptToScriptGenerator::Generate(
    const BehaviorPrompt& prompt, const std::filesystem::path& outputRoot) const
{
    BehaviorScript script{};
    script.id = prompt.id;
    script.version = prompt.version;
    script.generatedPath = outputRoot.empty()
                               ? std::filesystem::path()
                               : outputRoot / (SanitizeFileStem(prompt.id) + ".py");

    std::ostringstream builder;
    builder << "# Auto-generated by Aetherion semantic scripting\n";
    builder << "# Prompt name: " << (prompt.name.empty() ? prompt.id : prompt.name)
            << "\n\n";
    builder << "PROMPT = '''" << EscapeForTripleQuote(prompt.promptText) << "'''\n\n";
    builder << "def update(entity, context):\n";
    builder << "    \"\"\"\n";
    builder << "    Stub behavior generated from PROMPT.\n";
    builder << "    Replace with model-generated logic or edit the prompt to regenerate.\n";
    builder << "    \"\"\"\n";
    builder << "    return {\"state\": \"Idle\", \"actions\": []}\n";

    script.code = builder.str();
    script.diagnostics = "Generated stub behavior script";

    if (!script.generatedPath.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(script.generatedPath.parent_path(),
                                            ec);
        std::ofstream output(script.generatedPath, std::ios::trunc);
        if (output.is_open()) {
            output << script.code;
        }
    }

    return script;
}

} // namespace Aetherion::Scripting
