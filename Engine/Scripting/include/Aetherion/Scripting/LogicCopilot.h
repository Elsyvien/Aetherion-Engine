#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace Aetherion::Scripting
{

// ============================================================================
// Code Generation Types
// ============================================================================

enum class CodeGenerationStatus
{
    Pending,
    Generating,
    Validating,
    Compiling,
    Ready,
    Failed
};

struct GeneratedCode
{
    std::string className;
    std::string headerCode;
    std::string sourceCode;
    std::string prompt;
    std::string systemType;  // "System", "Component", "Behavior"
    
    // Validation results
    bool syntaxValid{false};
    bool compilesSuccessfully{false};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // File paths if saved
    std::filesystem::path headerPath;
    std::filesystem::path sourcePath;
};

struct CodeGenerationRequest
{
    std::string prompt;
    std::string systemType{"System"};  // System, Component, Behavior
    std::string targetName;            // Optional suggested name
    bool autoCompile{false};           // Try to compile after generation
    bool addToProject{false};          // Add to CMakeLists.txt
};

struct CodeGenerationResult
{
    std::string requestId;
    CodeGenerationStatus status{CodeGenerationStatus::Pending};
    GeneratedCode code;
    std::string statusMessage;
    float progress{0.0f};
};

// ============================================================================
// Code Templates
// ============================================================================

class CodeTemplates
{
public:
    static std::string GetSystemHeaderTemplate();
    static std::string GetSystemSourceTemplate();
    static std::string GetComponentHeaderTemplate();
    static std::string GetComponentSourceTemplate();
    static std::string GetBehaviorHeaderTemplate();
    static std::string GetBehaviorSourceTemplate();
    
    // Template variable replacement
    static std::string FillTemplate(const std::string& templ,
                                    const std::unordered_map<std::string, std::string>& vars);
};

// ============================================================================
// Logic Copilot - Main Interface
// ============================================================================

class LogicCopilot
{
public:
    using ProgressCallback = std::function<void(const std::string& requestId, float progress, const std::string& status)>;
    using CompletionCallback = std::function<void(const CodeGenerationResult& result)>;

    LogicCopilot();
    ~LogicCopilot();

    // Configuration
    void SetLLMEndpoint(const std::string& endpoint);
    void SetLLMApiKey(const std::string& apiKey);
    void SetLLMModel(const std::string& model);
    void SetOutputDirectory(const std::filesystem::path& dir);
    void SetProjectRoot(const std::filesystem::path& root);

    // Code Generation
    std::string GenerateCode(const CodeGenerationRequest& request,
                             ProgressCallback progressCb = nullptr,
                             CompletionCallback completionCb = nullptr);

    // Synchronous generation (blocks)
    CodeGenerationResult GenerateCodeSync(const CodeGenerationRequest& request);

    // Query status
    [[nodiscard]] const CodeGenerationResult* GetResult(const std::string& requestId) const;
    [[nodiscard]] std::vector<std::string> GetPendingRequests() const;

    // Code Validation
    bool ValidateSyntax(const GeneratedCode& code);
    bool TryCompile(const GeneratedCode& code, std::vector<std::string>& errors);

    // Code Management
    bool SaveGeneratedCode(const GeneratedCode& code);
    bool AddToProject(const GeneratedCode& code);
    bool RemoveFromProject(const std::string& className);

    // Prompt Enhancement
    std::string EnhancePrompt(const std::string& userPrompt, const std::string& systemType);

    // Examples and context
    void AddCodeExample(const std::string& name, const std::string& code);
    void SetContextCode(const std::string& context);

private:
    std::string CallLLM(const std::string& systemPrompt, const std::string& userPrompt);
    GeneratedCode ParseLLMResponse(const std::string& response, const std::string& systemType);
    std::string BuildSystemPrompt(const std::string& systemType);
    std::string GenerateClassName(const std::string& prompt);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// Code Validator - Static Analysis
// ============================================================================

class CodeValidator
{
public:
    struct ValidationResult
    {
        bool valid{false};
        std::vector<std::string> syntaxErrors;
        std::vector<std::string> semanticWarnings;
        std::vector<std::string> styleIssues;
    };

    // Basic syntax validation (bracket matching, etc.)
    static ValidationResult ValidateCppSyntax(const std::string& code);

    // Check for common issues in generated code
    static ValidationResult ValidateGeneratedSystem(const GeneratedCode& code);

    // Check if code follows Aetherion conventions
    static ValidationResult ValidateConventions(const GeneratedCode& code);
};

// ============================================================================
// Code Compiler - Build Integration
// ============================================================================

class CodeCompiler
{
public:
    struct CompileResult
    {
        bool success{false};
        std::string output;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::filesystem::path objectFile;
    };

    explicit CodeCompiler(const std::filesystem::path& projectRoot);

    // Test compile a single file
    CompileResult TestCompile(const std::filesystem::path& sourceFile);

    // Compile and link into shared library for hot-reload
    CompileResult CompileToLibrary(const std::vector<std::filesystem::path>& sources,
                                   const std::filesystem::path& outputLib);

    // Update CMakeLists.txt to include new source
    bool AddSourceToCMake(const std::filesystem::path& sourceFile);
    bool RemoveSourceFromCMake(const std::filesystem::path& sourceFile);

    // Full project rebuild
    CompileResult RebuildProject();

private:
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_buildDir;
    std::string m_compiler;
};

} // namespace Aetherion::Scripting
