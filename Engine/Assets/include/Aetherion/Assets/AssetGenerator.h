#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aetherion::Assets {

/// @brief Result of an asset generation operation
struct GenerationResult {
    bool success{false};
    std::string assetId;
    std::filesystem::path outputPath;
    std::string message;
    std::string diagnostics;
    std::uint64_t generationTimeMs{0};
};

/// @brief Configuration for a generation request
struct GenerationRequest {
    std::string requestId;
    std::string prompt;
    std::string assetType;  // "texture", "mesh", "audio", "script"
    std::string targetId;   // Optional pre-assigned asset ID
    std::filesystem::path outputDir;
    std::unordered_map<std::string, std::string> parameters;
    
    // Generator hints
    int width{512};
    int height{512};
    std::string format{"png"};
    float quality{0.9f};
    bool generateMipmaps{true};
    bool srgb{true};
};

/// @brief Status of a generation request
enum class GenerationStatus {
    Pending,
    InProgress,
    Completed,
    Failed,
    Cancelled
};

/// @brief Tracks the state of a generation request
struct GenerationState {
    std::string requestId;
    GenerationStatus status{GenerationStatus::Pending};
    float progress{0.0f};
    std::string statusMessage;
    std::optional<GenerationResult> result;
    std::uint64_t queuedTime{0};
    std::uint64_t startTime{0};
    std::uint64_t endTime{0};
    int retryCount{0};
    int maxRetries{3};
};

/// @brief Abstract base class for asset generators
///
/// Generators are pluggable backends that can create assets from prompts.
/// Implementations can use local models, remote APIs, or stub generation.
class IAssetGenerator {
public:
    virtual ~IAssetGenerator() = default;

    /// @brief Get the unique name of this generator
    [[nodiscard]] virtual std::string GetName() const = 0;

    /// @brief Get supported asset types (e.g., "texture", "mesh")
    [[nodiscard]] virtual std::vector<std::string> GetSupportedTypes() const = 0;

    /// @brief Check if this generator supports a specific asset type
    [[nodiscard]] virtual bool SupportsType(const std::string& assetType) const = 0;

    /// @brief Check if the generator is available and ready
    [[nodiscard]] virtual bool IsAvailable() const = 0;

    /// @brief Get a description of this generator
    [[nodiscard]] virtual std::string GetDescription() const = 0;

    /// @brief Generate an asset synchronously
    /// @param request The generation request
    /// @return Result of the generation
    [[nodiscard]] virtual GenerationResult Generate(
        const GenerationRequest& request) = 0;

    /// @brief Estimate generation time in milliseconds
    [[nodiscard]] virtual std::uint64_t EstimateTimeMs(
        const GenerationRequest& request) const {
        (void)request;
        return 1000;  // Default 1 second estimate
    }

    /// @brief Called when the generator is registered
    virtual void OnRegister() {}

    /// @brief Called when the generator is unregistered
    virtual void OnUnregister() {}
};

/// @brief Stub generator that creates placeholder assets
///
/// Useful for testing and prototyping without real generation backends.
class StubAssetGenerator : public IAssetGenerator {
public:
    StubAssetGenerator() = default;
    ~StubAssetGenerator() override = default;

    [[nodiscard]] std::string GetName() const override {
        return "StubGenerator";
    }

    [[nodiscard]] std::vector<std::string> GetSupportedTypes() const override {
        return {"texture", "mesh", "audio", "script"};
    }

    [[nodiscard]] bool SupportsType(const std::string& assetType) const override;
    [[nodiscard]] bool IsAvailable() const override { return true; }
    
    [[nodiscard]] std::string GetDescription() const override {
        return "Stub generator that creates placeholder assets for testing";
    }

    [[nodiscard]] GenerationResult Generate(
        const GenerationRequest& request) override;

    [[nodiscard]] std::uint64_t EstimateTimeMs(
        const GenerationRequest& request) const override {
        (void)request;
        return 100;  // Fast stub generation
    }

private:
    GenerationResult GenerateStubTexture(const GenerationRequest& request);
    GenerationResult GenerateStubMesh(const GenerationRequest& request);
    GenerationResult GenerateStubAudio(const GenerationRequest& request);
    GenerationResult GenerateStubScript(const GenerationRequest& request);
};

/// @brief Procedural texture generator using simple algorithms
///
/// Generates textures using procedural patterns (noise, gradients, patterns).
class ProceduralTextureGenerator : public IAssetGenerator {
public:
    ProceduralTextureGenerator() = default;
    ~ProceduralTextureGenerator() override = default;

    [[nodiscard]] std::string GetName() const override {
        return "ProceduralTextureGenerator";
    }

    [[nodiscard]] std::vector<std::string> GetSupportedTypes() const override {
        return {"texture"};
    }

    [[nodiscard]] bool SupportsType(const std::string& assetType) const override {
        return assetType == "texture";
    }

    [[nodiscard]] bool IsAvailable() const override { return true; }

    [[nodiscard]] std::string GetDescription() const override {
        return "Generates procedural textures from prompt keywords";
    }

    [[nodiscard]] GenerationResult Generate(
        const GenerationRequest& request) override;

private:
    enum class PatternType {
        Solid,
        Checker,
        Gradient,
        Noise,
        Brick,
        Grid,
        Stripes,
        Dots
    };

    PatternType ParsePatternFromPrompt(const std::string& prompt) const;
    std::array<uint8_t, 4> ParseColorFromPrompt(const std::string& prompt) const;
    void GeneratePattern(std::vector<uint8_t>& pixels, int width, int height,
                        PatternType pattern, const std::array<uint8_t, 4>& color1,
                        const std::array<uint8_t, 4>& color2) const;
};

/// @brief Manages a queue of generation requests with async execution
class GenerationQueue {
public:
    using ProgressCallback = std::function<void(const std::string& requestId,
                                                 float progress,
                                                 const std::string& message)>;
    using CompletionCallback = std::function<void(const GenerationResult& result)>;

    GenerationQueue();
    ~GenerationQueue();

    GenerationQueue(const GenerationQueue&) = delete;
    GenerationQueue& operator=(const GenerationQueue&) = delete;

    /// @brief Register a generator for a specific asset type
    void RegisterGenerator(std::shared_ptr<IAssetGenerator> generator);

    /// @brief Unregister a generator by name
    void UnregisterGenerator(const std::string& name);

    /// @brief Get a generator by name
    [[nodiscard]] std::shared_ptr<IAssetGenerator> GetGenerator(
        const std::string& name) const;

    /// @brief Get the best generator for an asset type
    [[nodiscard]] std::shared_ptr<IAssetGenerator> GetGeneratorForType(
        const std::string& assetType) const;

    /// @brief Queue a generation request
    /// @return Request ID for tracking
    std::string QueueRequest(GenerationRequest request,
                            CompletionCallback onComplete = nullptr);

    /// @brief Cancel a pending request
    bool CancelRequest(const std::string& requestId);

    /// @brief Retry a failed request
    bool RetryRequest(const std::string& requestId);

    /// @brief Get the state of a request
    [[nodiscard]] std::optional<GenerationState> GetRequestState(
        const std::string& requestId) const;

    /// @brief Get all pending request IDs
    [[nodiscard]] std::vector<std::string> GetPendingRequests() const;

    /// @brief Get all completed request IDs
    [[nodiscard]] std::vector<std::string> GetCompletedRequests() const;

    /// @brief Get all failed request IDs
    [[nodiscard]] std::vector<std::string> GetFailedRequests() const;

    /// @brief Process the next item in the queue (call from main/worker thread)
    /// @return true if an item was processed
    bool ProcessNext();

    /// @brief Process all pending items
    void ProcessAll();

    /// @brief Set progress callback for real-time updates
    void SetProgressCallback(ProgressCallback callback) {
        m_progressCallback = std::move(callback);
    }

    /// @brief Clear completed and failed requests from history
    void ClearHistory();

    /// @brief Get queue statistics
    [[nodiscard]] size_t GetPendingCount() const;
    [[nodiscard]] size_t GetCompletedCount() const;
    [[nodiscard]] size_t GetFailedCount() const;

    /// @brief Set the output directory for generated assets
    void SetOutputDirectory(std::filesystem::path dir);
    [[nodiscard]] const std::filesystem::path& GetOutputDirectory() const noexcept {
        return m_outputDirectory;
    }

private:
    std::string GenerateRequestId();
    void UpdateState(const std::string& requestId, GenerationStatus status,
                    float progress = 0.0f, const std::string& message = {});
    void NotifyProgress(const std::string& requestId, float progress,
                       const std::string& message);

    mutable std::mutex m_mutex;
    std::queue<std::string> m_pendingQueue;
    std::unordered_map<std::string, GenerationRequest> m_requests;
    std::unordered_map<std::string, GenerationState> m_states;
    std::unordered_map<std::string, CompletionCallback> m_callbacks;
    std::vector<std::shared_ptr<IAssetGenerator>> m_generators;
    std::filesystem::path m_outputDirectory;
    ProgressCallback m_progressCallback;
    std::uint64_t m_nextRequestId{1};
};

/// @brief Factory for creating generators
class AssetGeneratorFactory {
public:
    using GeneratorCreator = std::function<std::shared_ptr<IAssetGenerator>()>;

    static AssetGeneratorFactory& Instance();

    void RegisterCreator(const std::string& name, GeneratorCreator creator);
    [[nodiscard]] std::shared_ptr<IAssetGenerator> Create(
        const std::string& name) const;
    [[nodiscard]] std::vector<std::string> GetAvailableGenerators() const;

private:
    AssetGeneratorFactory() = default;
    std::unordered_map<std::string, GeneratorCreator> m_creators;
};

} // namespace Aetherion::Assets
