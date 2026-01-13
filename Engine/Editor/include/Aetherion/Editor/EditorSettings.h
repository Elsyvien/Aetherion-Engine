#pragma once

#include <algorithm>
#include <string>

namespace Aetherion::Editor
{

/// @brief LLM Provider options for asset generation
enum class LLMProviderType
{
    None = 0,       ///< Disabled - use procedural generation
    OpenAI,         ///< OpenAI API (GPT-4, DALL-E)
    Anthropic,      ///< Anthropic (Claude)
    StabilityAI,    ///< Stability AI (Stable Diffusion)
    LocalOllama,    ///< Local Ollama instance
    Custom          ///< Custom API endpoint
};

/// @brief LLM configuration for AI-powered asset generation
struct LLMSettings
{
    LLMProviderType provider{LLMProviderType::None};
    std::string apiKey;
    std::string endpoint;           ///< Custom endpoint URL
    std::string model;              ///< Text model name
    std::string imageModel;         ///< Image generation model
    int timeoutMs{60000};           ///< Request timeout
    bool enableLogging{false};      ///< Log API requests
};

struct EditorSettings
{
    bool validationEnabled{true};
    bool verboseLogging{true};
    bool vsyncEnabled{true};
    int targetFps{60};
    int headlessSleepMs{50};
    
    // LLM/AI settings for asset generation
    LLMSettings llm;

    void Clamp()
    {
        targetFps = std::clamp(targetFps, 1, 240);
        headlessSleepMs = std::clamp(headlessSleepMs, 0, 1000);
        llm.timeoutMs = std::clamp(llm.timeoutMs, 5000, 300000);
    }

    void Save() const;
    static EditorSettings Load();
};
} // namespace Aetherion::Editor
