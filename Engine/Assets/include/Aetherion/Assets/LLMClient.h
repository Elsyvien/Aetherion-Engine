#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aetherion::Assets {

/// @brief Supported LLM/AI providers
enum class LLMProvider {
    OpenAI,         ///< OpenAI API (GPT-4, DALL-E, etc.)
    Anthropic,      ///< Anthropic API (Claude)
    Google,         ///< Google AI (Gemini)
    Replicate,      ///< Replicate (various models)
    StabilityAI,    ///< Stability AI (Stable Diffusion)
    LocalOllama,    ///< Local Ollama instance
    Custom          ///< Custom endpoint
};

/// @brief Configuration for an LLM API connection
struct LLMConfig {
    LLMProvider provider{LLMProvider::OpenAI};
    std::string apiKey;
    std::string endpoint;           ///< Base API endpoint (auto-filled for known providers)
    std::string model;              ///< Model name (e.g., "gpt-4", "claude-3-opus")
    std::string imageModel;         ///< Image generation model (e.g., "dall-e-3")
    int timeoutMs{60000};           ///< Request timeout in milliseconds
    int maxRetries{3};              ///< Maximum retry attempts
    bool enableLogging{false};      ///< Log API requests/responses
    
    /// Get the default endpoint for a provider
    static std::string GetDefaultEndpoint(LLMProvider provider);
    
    /// Get the default model for a provider
    static std::string GetDefaultModel(LLMProvider provider);
    
    /// Get the default image model for a provider
    static std::string GetDefaultImageModel(LLMProvider provider);
};

/// @brief Response from an LLM text completion
struct LLMTextResponse {
    bool success{false};
    std::string content;
    std::string errorMessage;
    int statusCode{0};
    std::uint64_t promptTokens{0};
    std::uint64_t completionTokens{0};
    std::uint64_t latencyMs{0};
};

/// @brief Response from an image generation request
struct LLMImageResponse {
    bool success{false};
    std::vector<std::vector<uint8_t>> images;  ///< Raw image data (PNG)
    std::vector<std::string> imageUrls;         ///< URLs if not downloaded
    std::string errorMessage;
    int statusCode{0};
    std::string revisedPrompt;                  ///< Revised prompt (if model changed it)
    std::uint64_t latencyMs{0};
};

/// @brief Request for text completion
struct LLMTextRequest {
    std::string systemPrompt;
    std::string userPrompt;
    float temperature{0.7f};
    int maxTokens{4096};
    std::vector<std::string> stopSequences;
};

/// @brief Request for image generation
struct LLMImageRequest {
    std::string prompt;
    int width{1024};
    int height{1024};
    int numImages{1};
    std::string style;              ///< "natural", "vivid", etc.
    std::string quality;            ///< "standard", "hd", etc.
};

/// @brief Abstract LLM client interface
///
/// Provides a unified interface for communicating with various LLM providers.
/// The implementation uses Qt Network for HTTP requests.
class ILLMClient {
public:
    virtual ~ILLMClient() = default;

    /// @brief Initialize the client with configuration
    virtual bool Initialize(const LLMConfig& config) = 0;

    /// @brief Check if the client is ready to make requests
    [[nodiscard]] virtual bool IsReady() const = 0;

    /// @brief Get the current configuration
    [[nodiscard]] virtual const LLMConfig& GetConfig() const = 0;

    /// @brief Test the API connection
    [[nodiscard]] virtual bool TestConnection() = 0;

    /// @brief Generate text completion synchronously
    [[nodiscard]] virtual LLMTextResponse GenerateText(
        const LLMTextRequest& request) = 0;

    /// @brief Generate image synchronously
    [[nodiscard]] virtual LLMImageResponse GenerateImage(
        const LLMImageRequest& request) = 0;

    /// @brief Generate structured JSON output
    [[nodiscard]] virtual LLMTextResponse GenerateJSON(
        const std::string& systemPrompt,
        const std::string& userPrompt,
        const std::string& jsonSchema = "") = 0;
};

/// @brief Factory for creating LLM clients
class LLMClientFactory {
public:
    /// @brief Create a client for the specified provider
    static std::unique_ptr<ILLMClient> Create(LLMProvider provider);
    
    /// @brief Create a client from configuration
    static std::unique_ptr<ILLMClient> Create(const LLMConfig& config);
    
    /// @brief Get a list of available providers
    static std::vector<std::pair<LLMProvider, std::string>> GetAvailableProviders();
};

/// @brief OpenAI-compatible LLM client
///
/// Works with OpenAI API and compatible endpoints (Azure OpenAI, local servers, etc.)
class OpenAIClient : public ILLMClient {
public:
    OpenAIClient() = default;
    ~OpenAIClient() override = default;

    bool Initialize(const LLMConfig& config) override;
    [[nodiscard]] bool IsReady() const override;
    [[nodiscard]] const LLMConfig& GetConfig() const override { return m_config; }
    [[nodiscard]] bool TestConnection() override;
    [[nodiscard]] LLMTextResponse GenerateText(const LLMTextRequest& request) override;
    [[nodiscard]] LLMImageResponse GenerateImage(const LLMImageRequest& request) override;
    [[nodiscard]] LLMTextResponse GenerateJSON(
        const std::string& systemPrompt,
        const std::string& userPrompt,
        const std::string& jsonSchema = "") override;

private:
    struct HttpResponse {
        int statusCode{0};
        std::string body;
        std::string error;
    };
    
    HttpResponse DoHttpPost(const std::string& url,
                           const std::string& body,
                           const std::unordered_map<std::string, std::string>& headers);
    
    HttpResponse DoHttpGet(const std::string& url,
                          const std::unordered_map<std::string, std::string>& headers);
    
    std::vector<uint8_t> DownloadImage(const std::string& url);

    LLMConfig m_config;
    bool m_initialized{false};
};

/// @brief Anthropic Claude client
class AnthropicClient : public ILLMClient {
public:
    AnthropicClient() = default;
    ~AnthropicClient() override = default;

    bool Initialize(const LLMConfig& config) override;
    [[nodiscard]] bool IsReady() const override;
    [[nodiscard]] const LLMConfig& GetConfig() const override { return m_config; }
    [[nodiscard]] bool TestConnection() override;
    [[nodiscard]] LLMTextResponse GenerateText(const LLMTextRequest& request) override;
    [[nodiscard]] LLMImageResponse GenerateImage(const LLMImageRequest& request) override;
    [[nodiscard]] LLMTextResponse GenerateJSON(
        const std::string& systemPrompt,
        const std::string& userPrompt,
        const std::string& jsonSchema = "") override;

private:
    struct HttpResponse {
        int statusCode{0};
        std::string body;
        std::string error;
    };
    
    HttpResponse DoHttpPost(const std::string& url,
                           const std::string& body,
                           const std::unordered_map<std::string, std::string>& headers);

    LLMConfig m_config;
    bool m_initialized{false};
};

/// @brief Stability AI client for image generation
class StabilityAIClient : public ILLMClient {
public:
    StabilityAIClient() = default;
    ~StabilityAIClient() override = default;

    bool Initialize(const LLMConfig& config) override;
    [[nodiscard]] bool IsReady() const override;
    [[nodiscard]] const LLMConfig& GetConfig() const override { return m_config; }
    [[nodiscard]] bool TestConnection() override;
    [[nodiscard]] LLMTextResponse GenerateText(const LLMTextRequest& request) override;
    [[nodiscard]] LLMImageResponse GenerateImage(const LLMImageRequest& request) override;
    [[nodiscard]] LLMTextResponse GenerateJSON(
        const std::string& systemPrompt,
        const std::string& userPrompt,
        const std::string& jsonSchema = "") override;

private:
    struct HttpResponse {
        int statusCode{0};
        std::string body;
        std::string error;
    };
    
    HttpResponse DoHttpPost(const std::string& url,
                           const std::string& body,
                           const std::unordered_map<std::string, std::string>& headers);

    LLMConfig m_config;
    bool m_initialized{false};
};

} // namespace Aetherion::Assets
