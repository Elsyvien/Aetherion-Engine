#include "Aetherion/Scripting/LogicCopilot.h"
#include "Aetherion/Assets/LLMClient.h"
#include <nlohmann/json.hpp>
#include <regex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>

namespace Aetherion::Scripting
{

using json = nlohmann::json;

// ============================================================================
// Code Templates Implementation
// ============================================================================

std::string CodeTemplates::GetSystemHeaderTemplate()
{
    return R"(#pragma once

#include "Aetherion/Scene/System.h"
#include <string>

namespace Aetherion::Scene
{

/// @brief {{DESCRIPTION}}
/// Generated from prompt: "{{PROMPT}}"
class {{CLASS_NAME}} : public System
{
public:
    {{CLASS_NAME}}() = default;
    ~{{CLASS_NAME}}() override = default;

    [[nodiscard]] std::string GetName() const override { return "{{CLASS_NAME}}"; }
    void Configure(Runtime::EngineContext& context) override;
    void Update(Scene& scene, float deltaTime) override;

{{CUSTOM_MEMBERS}}
private:
{{PRIVATE_MEMBERS}}
};

} // namespace Aetherion::Scene
)";
}

std::string CodeTemplates::GetSystemSourceTemplate()
{
    return R"(#include "{{HEADER_PATH}}"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Runtime/EngineContext.h"

namespace Aetherion::Scene
{

void {{CLASS_NAME}}::Configure(Runtime::EngineContext& context)
{
{{CONFIGURE_BODY}}
}

void {{CLASS_NAME}}::Update(Scene& scene, float deltaTime)
{
{{UPDATE_BODY}}
}

{{ADDITIONAL_METHODS}}

} // namespace Aetherion::Scene
)";
}

std::string CodeTemplates::GetComponentHeaderTemplate()
{
    return R"(#pragma once

#include "Aetherion/Scene/Component.h"
#include <string>

namespace Aetherion::Scene
{

/// @brief {{DESCRIPTION}}
/// Generated from prompt: "{{PROMPT}}"
class {{CLASS_NAME}} : public Component
{
public:
    {{CLASS_NAME}}() = default;
    ~{{CLASS_NAME}}() override = default;

    [[nodiscard]] std::string GetDisplayName() const override { return "{{DISPLAY_NAME}}"; }

{{CUSTOM_MEMBERS}}
protected:
    void OnAdded() override;
    void OnRemoved() override;
    void OnUpdate(float deltaTime) override;

private:
{{PRIVATE_MEMBERS}}
};

} // namespace Aetherion::Scene
)";
}

std::string CodeTemplates::GetComponentSourceTemplate()
{
    return R"(#include "{{HEADER_PATH}}"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Scene
{

void {{CLASS_NAME}}::OnAdded()
{
{{ON_ADDED_BODY}}
}

void {{CLASS_NAME}}::OnRemoved()
{
{{ON_REMOVED_BODY}}
}

void {{CLASS_NAME}}::OnUpdate(float deltaTime)
{
{{UPDATE_BODY}}
}

{{ADDITIONAL_METHODS}}

} // namespace Aetherion::Scene
)";
}

std::string CodeTemplates::GetBehaviorHeaderTemplate()
{
    return R"(#pragma once

#include "Aetherion/Scene/Component.h"
#include <string>
#include <functional>

namespace Aetherion::Scene
{

/// @brief AI Behavior: {{DESCRIPTION}}
/// Generated from prompt: "{{PROMPT}}"
class {{CLASS_NAME}} : public Component
{
public:
    {{CLASS_NAME}}() = default;
    ~{{CLASS_NAME}}() override = default;

    [[nodiscard]] std::string GetDisplayName() const override { return "{{DISPLAY_NAME}}"; }

    // Behavior state
    enum class State { {{STATES}} };
    
    [[nodiscard]] State GetCurrentState() const noexcept { return m_currentState; }
    void SetState(State state) { m_currentState = state; }

{{CUSTOM_MEMBERS}}
protected:
    void OnUpdate(float deltaTime) override;

private:
    void UpdateBehavior(float deltaTime);
    State EvaluateTransitions();

    State m_currentState{State::{{INITIAL_STATE}}};
{{PRIVATE_MEMBERS}}
};

} // namespace Aetherion::Scene
)";
}

std::string CodeTemplates::GetBehaviorSourceTemplate()
{
    return R"(#include "{{HEADER_PATH}}"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Scene
{

void {{CLASS_NAME}}::OnUpdate(float deltaTime)
{
    // Evaluate state transitions
    State newState = EvaluateTransitions();
    if (newState != m_currentState)
    {
        m_currentState = newState;
    }
    
    UpdateBehavior(deltaTime);
}

void {{CLASS_NAME}}::UpdateBehavior(float deltaTime)
{
    switch (m_currentState)
    {
{{STATE_UPDATE_CASES}}
    }
}

{{CLASS_NAME}}::State {{CLASS_NAME}}::EvaluateTransitions()
{
{{TRANSITION_LOGIC}}
    return m_currentState;
}

{{ADDITIONAL_METHODS}}

} // namespace Aetherion::Scene
)";
}

std::string CodeTemplates::FillTemplate(const std::string& templ,
                                         const std::unordered_map<std::string, std::string>& vars)
{
    std::string result = templ;
    for (const auto& [key, value] : vars)
    {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos)
        {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    return result;
}

// ============================================================================
// LogicCopilot Implementation
// ============================================================================

struct LogicCopilot::Impl
{
    std::string llmEndpoint{"https://api.openai.com/v1"};
    std::string llmApiKey;
    std::string llmModel{"gpt-4"};
    std::filesystem::path outputDir;
    std::filesystem::path projectRoot;
    
    std::unordered_map<std::string, CodeGenerationResult> results;
    std::unordered_map<std::string, std::string> codeExamples;
    std::string contextCode;
    
    std::unique_ptr<Assets::ILLMClient> llmClient;
    
    uint64_t nextRequestId{1};
};

LogicCopilot::LogicCopilot() : m_impl(std::make_unique<Impl>())
{
}

LogicCopilot::~LogicCopilot() = default;

void LogicCopilot::SetLLMEndpoint(const std::string& endpoint)
{
    m_impl->llmEndpoint = endpoint;
    m_impl->llmClient.reset();
}

void LogicCopilot::SetLLMApiKey(const std::string& apiKey)
{
    m_impl->llmApiKey = apiKey;
    m_impl->llmClient.reset();
}

void LogicCopilot::SetLLMModel(const std::string& model)
{
    m_impl->llmModel = model;
}

void LogicCopilot::SetOutputDirectory(const std::filesystem::path& dir)
{
    m_impl->outputDir = dir;
    std::filesystem::create_directories(dir);
}

void LogicCopilot::SetProjectRoot(const std::filesystem::path& root)
{
    m_impl->projectRoot = root;
}

std::string LogicCopilot::GenerateCode(const CodeGenerationRequest& request,
                                        ProgressCallback progressCb,
                                        CompletionCallback completionCb)
{
    std::string requestId = "codegen_" + std::to_string(m_impl->nextRequestId++);
    
    CodeGenerationResult result;
    result.requestId = requestId;
    result.status = CodeGenerationStatus::Pending;
    result.code.prompt = request.prompt;
    result.code.systemType = request.systemType;
    m_impl->results[requestId] = result;

    // For now, do synchronous generation (can be made async with QtConcurrent)
    if (progressCb) progressCb(requestId, 0.1f, "Building prompt...");
    
    result.status = CodeGenerationStatus::Generating;
    m_impl->results[requestId] = result;
    
    std::string enhancedPrompt = EnhancePrompt(request.prompt, request.systemType);
    std::string systemPrompt = BuildSystemPrompt(request.systemType);
    
    if (progressCb) progressCb(requestId, 0.2f, "Calling LLM...");
    
    std::string llmResponse = CallLLM(systemPrompt, enhancedPrompt);
    
    if (llmResponse.empty())
    {
        result.status = CodeGenerationStatus::Failed;
        result.statusMessage = "LLM returned empty response";
        result.code.errors.push_back(result.statusMessage);
        m_impl->results[requestId] = result;
        if (completionCb) completionCb(result);
        return requestId;
    }
    
    if (progressCb) progressCb(requestId, 0.5f, "Parsing response...");
    
    result.code = ParseLLMResponse(llmResponse, request.systemType);
    result.code.prompt = request.prompt;
    result.code.systemType = request.systemType;
    
    // Override class name if specified
    if (!request.targetName.empty())
    {
        result.code.className = request.targetName;
    }
    
    if (progressCb) progressCb(requestId, 0.7f, "Validating code...");
    
    result.status = CodeGenerationStatus::Validating;
    m_impl->results[requestId] = result;
    
    result.code.syntaxValid = ValidateSyntax(result.code);
    
    if (!result.code.syntaxValid)
    {
        result.status = CodeGenerationStatus::Failed;
        result.statusMessage = "Generated code has syntax errors";
        m_impl->results[requestId] = result;
        if (completionCb) completionCb(result);
        return requestId;
    }
    
    // Save the code
    if (progressCb) progressCb(requestId, 0.8f, "Saving code...");
    
    if (!m_impl->outputDir.empty())
    {
        SaveGeneratedCode(result.code);
    }
    
    // Try to compile if requested
    if (request.autoCompile)
    {
        if (progressCb) progressCb(requestId, 0.9f, "Compiling...");
        result.status = CodeGenerationStatus::Compiling;
        m_impl->results[requestId] = result;
        
        std::vector<std::string> compileErrors;
        result.code.compilesSuccessfully = TryCompile(result.code, compileErrors);
        result.code.errors.insert(result.code.errors.end(), compileErrors.begin(), compileErrors.end());
    }
    
    // Add to project if requested
    if (request.addToProject && result.code.syntaxValid)
    {
        AddToProject(result.code);
    }
    
    result.status = result.code.syntaxValid ? CodeGenerationStatus::Ready : CodeGenerationStatus::Failed;
    result.progress = 1.0f;
    result.statusMessage = result.code.syntaxValid ? "Code generated successfully" : "Code generation failed";
    m_impl->results[requestId] = result;
    
    if (completionCb) completionCb(result);
    return requestId;
}

CodeGenerationResult LogicCopilot::GenerateCodeSync(const CodeGenerationRequest& request)
{
    std::string id = GenerateCode(request, nullptr, nullptr);
    auto* result = GetResult(id);
    return result ? *result : CodeGenerationResult{};
}

const CodeGenerationResult* LogicCopilot::GetResult(const std::string& requestId) const
{
    auto it = m_impl->results.find(requestId);
    return it != m_impl->results.end() ? &it->second : nullptr;
}

std::vector<std::string> LogicCopilot::GetPendingRequests() const
{
    std::vector<std::string> pending;
    for (const auto& [id, result] : m_impl->results)
    {
        if (result.status == CodeGenerationStatus::Pending ||
            result.status == CodeGenerationStatus::Generating ||
            result.status == CodeGenerationStatus::Validating ||
            result.status == CodeGenerationStatus::Compiling)
        {
            pending.push_back(id);
        }
    }
    return pending;
}

std::string LogicCopilot::CallLLM(const std::string& systemPrompt, const std::string& userPrompt)
{
    // Create LLM client if needed
    if (!m_impl->llmClient)
    {
        Assets::LLMConfig config;
        config.provider = Assets::LLMProvider::OpenAI;
        config.apiKey = m_impl->llmApiKey;
        config.endpoint = m_impl->llmEndpoint;
        config.model = m_impl->llmModel;
        
        m_impl->llmClient = Assets::LLMClientFactory::Create(config);
    }
    
    if (!m_impl->llmClient)
    {
        return "";
    }
    
    Assets::LLMTextRequest req;
    req.systemPrompt = systemPrompt;
    req.userPrompt = userPrompt;
    req.maxTokens = 4096;
    req.temperature = 0.2f; // Low temperature for more deterministic code
    
    Assets::LLMTextResponse resp = m_impl->llmClient->CompleteText(req);
    
    if (!resp.success)
    {
        return "";
    }
    
    return resp.text;
}

std::string LogicCopilot::BuildSystemPrompt(const std::string& systemType)
{
    std::stringstream ss;
    
    ss << "You are an expert C++ game engine developer. You are generating code for the Aetherion Engine, ";
    ss << "which uses an Entity-Component-System (ECS) architecture.\n\n";
    
    ss << "IMPORTANT RULES:\n";
    ss << "1. Generate complete, compilable C++ code\n";
    ss << "2. Use modern C++20 features where appropriate\n";
    ss << "3. Follow the Aetherion naming conventions:\n";
    ss << "   - Classes: PascalCase (e.g., PlayerController)\n";
    ss << "   - Methods: PascalCase (e.g., GetPosition)\n";
    ss << "   - Member variables: m_camelCase (e.g., m_speed)\n";
    ss << "   - Parameters: camelCase (e.g., deltaTime)\n";
    ss << "4. Include all necessary headers\n";
    ss << "5. Add documentation comments\n";
    ss << "6. Handle edge cases and errors gracefully\n\n";
    
    if (systemType == "System")
    {
        ss << "You are generating an ECS SYSTEM.\n";
        ss << "Systems inherit from Aetherion::Scene::System.\n";
        ss << "Required overrides: GetName(), Configure(), Update()\n";
        ss << "Systems iterate over entities with specific components.\n\n";
        
        ss << "Available components:\n";
        ss << "- TransformComponent: position, rotation, scale\n";
        ss << "- MeshRendererComponent: mesh rendering\n";
        ss << "- LightComponent: lighting\n";
        ss << "- CameraComponent: camera\n";
        ss << "- RigidbodyComponent: physics\n";
        ss << "- ColliderComponent: collision\n";
        ss << "- AnimatorComponent: animation\n";
        ss << "- AudioSourceComponent: audio\n\n";
    }
    else if (systemType == "Component")
    {
        ss << "You are generating an ECS COMPONENT.\n";
        ss << "Components inherit from Aetherion::Scene::Component.\n";
        ss << "Required overrides: GetDisplayName()\n";
        ss << "Optional overrides: OnAdded(), OnRemoved(), OnUpdate()\n\n";
    }
    else if (systemType == "Behavior")
    {
        ss << "You are generating a STATE MACHINE BEHAVIOR component.\n";
        ss << "Create a component with distinct states and transitions.\n";
        ss << "Use an enum for states and implement state logic.\n\n";
    }
    
    ss << "OUTPUT FORMAT:\n";
    ss << "Provide your response as a JSON object with these fields:\n";
    ss << "{\n";
    ss << "  \"className\": \"YourClassName\",\n";
    ss << "  \"description\": \"Brief description\",\n";
    ss << "  \"headerCode\": \"// Complete .h file content\",\n";
    ss << "  \"sourceCode\": \"// Complete .cpp file content\"\n";
    ss << "}\n\n";
    
    // Add examples if available
    if (!m_impl->codeExamples.empty())
    {
        ss << "EXAMPLES:\n";
        for (const auto& [name, code] : m_impl->codeExamples)
        {
            ss << "--- " << name << " ---\n" << code << "\n\n";
        }
    }
    
    return ss.str();
}

std::string LogicCopilot::EnhancePrompt(const std::string& userPrompt, const std::string& systemType)
{
    std::stringstream ss;
    
    ss << "Generate a " << systemType << " based on this description:\n\n";
    ss << userPrompt << "\n\n";
    
    if (!m_impl->contextCode.empty())
    {
        ss << "CONTEXT (existing code in project):\n";
        ss << m_impl->contextCode << "\n\n";
    }
    
    ss << "Remember to output valid JSON with className, description, headerCode, and sourceCode fields.";
    
    return ss.str();
}

GeneratedCode LogicCopilot::ParseLLMResponse(const std::string& response, const std::string& systemType)
{
    GeneratedCode code;
    code.systemType = systemType;
    
    try
    {
        // Try to find JSON in the response
        size_t jsonStart = response.find('{');
        size_t jsonEnd = response.rfind('}');
        
        if (jsonStart == std::string::npos || jsonEnd == std::string::npos)
        {
            code.errors.push_back("No JSON found in LLM response");
            return code;
        }
        
        std::string jsonStr = response.substr(jsonStart, jsonEnd - jsonStart + 1);
        json j = json::parse(jsonStr);
        
        code.className = j.value("className", "GeneratedCode");
        code.headerCode = j.value("headerCode", "");
        code.sourceCode = j.value("sourceCode", "");
        
        // Clean up code (remove markdown code blocks if present)
        auto cleanCode = [](std::string& s) {
            // Remove ```cpp and ``` markers
            std::regex codeBlockRegex("```(?:cpp)?\\n?");
            s = std::regex_replace(s, codeBlockRegex, "");
            
            // Trim
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            if (start != std::string::npos && end != std::string::npos)
            {
                s = s.substr(start, end - start + 1);
            }
        };
        
        cleanCode(code.headerCode);
        cleanCode(code.sourceCode);
    }
    catch (const std::exception& e)
    {
        code.errors.push_back(std::string("Failed to parse LLM response: ") + e.what());
        
        // Fallback: try to extract code from markdown blocks
        std::regex headerRegex("```(?:cpp|h)\\n([\\s\\S]*?)```");
        std::smatch match;
        
        std::string remaining = response;
        std::vector<std::string> codeBlocks;
        
        while (std::regex_search(remaining, match, headerRegex))
        {
            codeBlocks.push_back(match[1].str());
            remaining = match.suffix();
        }
        
        if (codeBlocks.size() >= 2)
        {
            code.headerCode = codeBlocks[0];
            code.sourceCode = codeBlocks[1];
            code.className = GenerateClassName(code.headerCode);
            code.errors.clear(); // Clear error if we recovered
        }
    }
    
    return code;
}

std::string LogicCopilot::GenerateClassName(const std::string& prompt)
{
    // Try to extract class name from code
    std::regex classRegex("class\\s+(\\w+)");
    std::smatch match;
    
    if (std::regex_search(prompt, match, classRegex))
    {
        return match[1].str();
    }
    
    // Generate from timestamp
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    
    return "Generated_" + std::to_string(millis % 100000);
}

bool LogicCopilot::ValidateSyntax(const GeneratedCode& code)
{
    auto result = CodeValidator::ValidateCppSyntax(code.headerCode);
    if (!result.valid)
    {
        return false;
    }
    
    result = CodeValidator::ValidateCppSyntax(code.sourceCode);
    return result.valid;
}

bool LogicCopilot::TryCompile(const GeneratedCode& code, std::vector<std::string>& errors)
{
    if (m_impl->projectRoot.empty())
    {
        errors.push_back("Project root not set");
        return false;
    }
    
    CodeCompiler compiler(m_impl->projectRoot);
    
    // Save to temp files
    auto tempDir = std::filesystem::temp_directory_path() / "aetherion_codegen";
    std::filesystem::create_directories(tempDir);
    
    auto headerPath = tempDir / (code.className + ".h");
    auto sourcePath = tempDir / (code.className + ".cpp");
    
    std::ofstream headerFile(headerPath);
    headerFile << code.headerCode;
    headerFile.close();
    
    std::ofstream sourceFile(sourcePath);
    sourceFile << code.sourceCode;
    sourceFile.close();
    
    auto result = compiler.TestCompile(sourcePath);
    errors = result.errors;
    
    return result.success;
}

bool LogicCopilot::SaveGeneratedCode(const GeneratedCode& code)
{
    if (m_impl->outputDir.empty())
    {
        return false;
    }
    
    auto headerPath = m_impl->outputDir / (code.className + ".h");
    auto sourcePath = m_impl->outputDir / (code.className + ".cpp");
    
    std::ofstream headerFile(headerPath);
    if (!headerFile.is_open()) return false;
    headerFile << code.headerCode;
    headerFile.close();
    
    std::ofstream sourceFile(sourcePath);
    if (!sourceFile.is_open()) return false;
    sourceFile << code.sourceCode;
    sourceFile.close();
    
    return true;
}

bool LogicCopilot::AddToProject(const GeneratedCode& code)
{
    if (m_impl->projectRoot.empty() || m_impl->outputDir.empty())
    {
        return false;
    }
    
    CodeCompiler compiler(m_impl->projectRoot);
    auto sourcePath = m_impl->outputDir / (code.className + ".cpp");
    
    return compiler.AddSourceToCMake(sourcePath);
}

bool LogicCopilot::RemoveFromProject(const std::string& className)
{
    if (m_impl->projectRoot.empty() || m_impl->outputDir.empty())
    {
        return false;
    }
    
    CodeCompiler compiler(m_impl->projectRoot);
    auto sourcePath = m_impl->outputDir / (className + ".cpp");
    
    return compiler.RemoveSourceFromCMake(sourcePath);
}

void LogicCopilot::AddCodeExample(const std::string& name, const std::string& code)
{
    m_impl->codeExamples[name] = code;
}

void LogicCopilot::SetContextCode(const std::string& context)
{
    m_impl->contextCode = context;
}

// ============================================================================
// CodeValidator Implementation
// ============================================================================

CodeValidator::ValidationResult CodeValidator::ValidateCppSyntax(const std::string& code)
{
    ValidationResult result;
    result.valid = true;
    
    // Check bracket matching
    int braceCount = 0;
    int parenCount = 0;
    int bracketCount = 0;
    bool inString = false;
    bool inChar = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    
    for (size_t i = 0; i < code.size(); ++i)
    {
        char c = code[i];
        char next = (i + 1 < code.size()) ? code[i + 1] : '\0';
        
        // Handle comments
        if (!inString && !inChar)
        {
            if (c == '/' && next == '/')
            {
                inLineComment = true;
                continue;
            }
            if (c == '/' && next == '*')
            {
                inBlockComment = true;
                continue;
            }
            if (inLineComment && c == '\n')
            {
                inLineComment = false;
                continue;
            }
            if (inBlockComment && c == '*' && next == '/')
            {
                inBlockComment = false;
                ++i;
                continue;
            }
        }
        
        if (inLineComment || inBlockComment) continue;
        
        // Handle strings
        if (c == '"' && !inChar && (i == 0 || code[i-1] != '\\'))
        {
            inString = !inString;
            continue;
        }
        if (c == '\'' && !inString && (i == 0 || code[i-1] != '\\'))
        {
            inChar = !inChar;
            continue;
        }
        
        if (inString || inChar) continue;
        
        // Count brackets
        if (c == '{') ++braceCount;
        else if (c == '}') --braceCount;
        else if (c == '(') ++parenCount;
        else if (c == ')') --parenCount;
        else if (c == '[') ++bracketCount;
        else if (c == ']') --bracketCount;
        
        if (braceCount < 0 || parenCount < 0 || bracketCount < 0)
        {
            result.valid = false;
            result.syntaxErrors.push_back("Unmatched closing bracket");
        }
    }
    
    if (braceCount != 0)
    {
        result.valid = false;
        result.syntaxErrors.push_back("Unmatched braces: " + std::to_string(braceCount) + " open");
    }
    if (parenCount != 0)
    {
        result.valid = false;
        result.syntaxErrors.push_back("Unmatched parentheses: " + std::to_string(parenCount) + " open");
    }
    if (bracketCount != 0)
    {
        result.valid = false;
        result.syntaxErrors.push_back("Unmatched brackets: " + std::to_string(bracketCount) + " open");
    }
    
    // Check for common issues
    if (code.find("class ") != std::string::npos && code.find("};") == std::string::npos)
    {
        result.semanticWarnings.push_back("Class declaration may be missing closing '};'");
    }
    
    return result;
}

CodeValidator::ValidationResult CodeValidator::ValidateGeneratedSystem(const GeneratedCode& code)
{
    ValidationResult result = ValidateCppSyntax(code.headerCode);
    
    if (!result.valid) return result;
    
    auto srcResult = ValidateCppSyntax(code.sourceCode);
    result.syntaxErrors.insert(result.syntaxErrors.end(), 
                               srcResult.syntaxErrors.begin(), 
                               srcResult.syntaxErrors.end());
    result.valid = result.valid && srcResult.valid;
    
    // Check for required components
    if (code.systemType == "System")
    {
        if (code.headerCode.find("GetName()") == std::string::npos)
        {
            result.semanticWarnings.push_back("System may be missing GetName() override");
        }
        if (code.sourceCode.find("Update(") == std::string::npos)
        {
            result.semanticWarnings.push_back("System may be missing Update() implementation");
        }
    }
    
    return result;
}

CodeValidator::ValidationResult CodeValidator::ValidateConventions(const GeneratedCode& code)
{
    ValidationResult result;
    result.valid = true;
    
    // Check naming conventions
    std::regex memberVarRegex("\\bm_[a-z]");
    if (!std::regex_search(code.headerCode, memberVarRegex) && 
        code.headerCode.find("private:") != std::string::npos)
    {
        result.styleIssues.push_back("Member variables should use m_camelCase naming");
    }
    
    return result;
}

// ============================================================================
// CodeCompiler Implementation
// ============================================================================

CodeCompiler::CodeCompiler(const std::filesystem::path& projectRoot)
    : m_projectRoot(projectRoot)
    , m_buildDir(projectRoot / "build-mingw")
{
#ifdef _WIN32
    m_compiler = "g++";
#else
    m_compiler = "g++";
#endif
}

CodeCompiler::CompileResult CodeCompiler::TestCompile(const std::filesystem::path& sourceFile)
{
    CompileResult result;
    
    // For now, just check if the file exists and has valid syntax
    if (!std::filesystem::exists(sourceFile))
    {
        result.errors.push_back("Source file does not exist");
        return result;
    }
    
    // Read the file
    std::ifstream file(sourceFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    auto validation = CodeValidator::ValidateCppSyntax(code);
    if (!validation.valid)
    {
        result.errors = validation.syntaxErrors;
        return result;
    }
    
    result.success = true;
    result.output = "Syntax validation passed";
    
    return result;
}

CodeCompiler::CompileResult CodeCompiler::CompileToLibrary(
    const std::vector<std::filesystem::path>& sources,
    const std::filesystem::path& outputLib)
{
    CompileResult result;
    result.success = false;
    result.errors.push_back("Hot-reload compilation not yet implemented");
    return result;
}

bool CodeCompiler::AddSourceToCMake(const std::filesystem::path& sourceFile)
{
    auto cmakePath = m_projectRoot / "CMakeLists.txt";
    if (!std::filesystem::exists(cmakePath))
    {
        return false;
    }
    
    // Read CMakeLists.txt
    std::ifstream inFile(cmakePath);
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string content = buffer.str();
    inFile.close();
    
    // Make path relative
    auto relPath = std::filesystem::relative(sourceFile, m_projectRoot);
    std::string relPathStr = relPath.string();
    std::replace(relPathStr.begin(), relPathStr.end(), '\\', '/');
    
    // Check if already present
    if (content.find(relPathStr) != std::string::npos)
    {
        return true; // Already added
    }
    
    // Find EDITOR_SOURCES or RUNTIME_SOURCES and add
    std::string searchStr = "set(RUNTIME_SOURCES";
    size_t pos = content.find(searchStr);
    if (pos == std::string::npos)
    {
        return false;
    }
    
    // Find the closing parenthesis
    size_t endPos = content.find(')', pos);
    if (endPos == std::string::npos)
    {
        return false;
    }
    
    // Insert before the closing paren
    std::string insertion = "    " + relPathStr + "\n";
    content.insert(endPos, insertion);
    
    // Write back
    std::ofstream outFile(cmakePath);
    outFile << content;
    
    return true;
}

bool CodeCompiler::RemoveSourceFromCMake(const std::filesystem::path& sourceFile)
{
    auto cmakePath = m_projectRoot / "CMakeLists.txt";
    if (!std::filesystem::exists(cmakePath))
    {
        return false;
    }
    
    // Read CMakeLists.txt
    std::ifstream inFile(cmakePath);
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string content = buffer.str();
    inFile.close();
    
    // Make path relative
    auto relPath = std::filesystem::relative(sourceFile, m_projectRoot);
    std::string relPathStr = relPath.string();
    std::replace(relPathStr.begin(), relPathStr.end(), '\\', '/');
    
    // Find and remove the line
    size_t pos = content.find(relPathStr);
    if (pos == std::string::npos)
    {
        return true; // Not present
    }
    
    // Find the start and end of the line
    size_t lineStart = content.rfind('\n', pos);
    size_t lineEnd = content.find('\n', pos);
    
    if (lineStart == std::string::npos) lineStart = 0;
    else lineStart++; // Skip the newline
    
    if (lineEnd == std::string::npos) lineEnd = content.size();
    else lineEnd++; // Include the newline
    
    content.erase(lineStart, lineEnd - lineStart);
    
    // Write back
    std::ofstream outFile(cmakePath);
    outFile << content;
    
    return true;
}

CodeCompiler::CompileResult CodeCompiler::RebuildProject()
{
    CompileResult result;
    result.success = false;
    result.errors.push_back("Full project rebuild not yet implemented");
    return result;
}

} // namespace Aetherion::Scripting
