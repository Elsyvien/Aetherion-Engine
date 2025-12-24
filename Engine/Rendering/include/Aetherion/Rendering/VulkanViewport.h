#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <vulkan/vulkan.h>

#include "Aetherion/Rendering/RenderView.h"

namespace Aetherion::Rendering
{
class VulkanContext;
}

namespace Aetherion::Assets
{
class AssetRegistry;
}

namespace Aetherion::Rendering
{
class VulkanViewport
{
public:
    explicit VulkanViewport(std::shared_ptr<VulkanContext> context,
                            std::shared_ptr<Assets::AssetRegistry> assetRegistry = nullptr);
    ~VulkanViewport();

    VulkanViewport(const VulkanViewport&) = delete;
    VulkanViewport& operator=(const VulkanViewport&) = delete;

    void Initialize(void* nativeHandle, int width, int height);
    void Resize(int width, int height);
    void RenderFrame(float deltaTimeSeconds, const RenderView& view);
    void Shutdown();

    [[nodiscard]] bool IsReady() const noexcept { return m_ready; }
    void SetLoggingEnabled(bool enabled) noexcept { m_verboseLogging = enabled; }

    // Camera control
    void SetCameraPosition(float x, float y, float z) noexcept;
    void SetCameraRotation(float yawDeg, float pitchDeg) noexcept;
    void SetCameraZoom(float zoom) noexcept;
    void SetCameraDistance(float distance) noexcept;
    void ResetCamera() noexcept;
    void FocusOnBounds(float centerX, float centerY, float centerZ, float radius, float padding = 1.25f) noexcept;

private:
    static constexpr uint32_t kMaxFramesInFlight = 2;
    static constexpr uint32_t kMaxTextureDescriptors = 128;
    struct InstancePushConstants
    {
        float model[16]{};
        float color[4]{};
    };

    struct DrawInstance
    {
        InstancePushConstants constants{};
        Core::EntityId entityId{0};
        std::string meshId;
        std::string textureId;
    };

    struct GpuMesh
    {
        VkBuffer vertexBuffer{VK_NULL_HANDLE};
        VkDeviceMemory vertexMemory{VK_NULL_HANDLE};
        VkBuffer indexBuffer{VK_NULL_HANDLE};
        VkDeviceMemory indexMemory{VK_NULL_HANDLE};
        uint32_t indexCount{0};
    };

    struct GpuTexture
    {
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
        uint32_t width{0};
        uint32_t height{0};
    };

    struct FrameUniform
    {
        float viewProj[16]{};
    };

    std::shared_ptr<VulkanContext> m_context;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    bool m_ready{false};
    bool m_verboseLogging{true};
    float m_timeSeconds{0.0f};
    bool m_waitingForValidExtent{false};
    bool m_shutdown{false};
    bool m_needsSwapchainRecreate{false};

    // Camera state
    float m_cameraX{0.0f};
    float m_cameraY{0.0f};
    float m_cameraZ{0.0f};
    float m_cameraYawDeg{30.0f};
    float m_cameraPitchDeg{25.0f};
    float m_cameraZoom{1.0f};
    float m_cameraDistance{5.0f};

    void* m_nativeHandle{nullptr};
    int m_surfaceWidth{0};
    int m_surfaceHeight{0};

#ifdef __APPLE__
    void* m_metalLayer{nullptr};  // Retained CAMetalLayer for macOS.
#endif

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_swapchainFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent{};
    VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
    VkImage m_depthImage{VK_NULL_HANDLE};
    VkDeviceMemory m_depthMemory{VK_NULL_HANDLE};
    VkImageView m_depthImageView{VK_NULL_HANDLE};

    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkRenderPass m_renderPass{VK_NULL_HANDLE};

    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_textureDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorPool m_textureDescriptorPool{VK_NULL_HANDLE};
    std::array<VkDescriptorSet, kMaxFramesInFlight> m_descriptorSets{};

    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipeline m_linePipeline{VK_NULL_HANDLE};
    VkPipeline m_overlayPipeline{VK_NULL_HANDLE};

    std::vector<VkFramebuffer> m_framebuffers;

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vertexMemory{VK_NULL_HANDLE};
    VkBuffer m_indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_indexMemory{VK_NULL_HANDLE};
    uint32_t m_defaultIndexCount{0};
    VkBuffer m_lineVertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_lineVertexMemory{VK_NULL_HANDLE};
    uint32_t m_lineVertexCount{0};
    VkBuffer m_selectionVertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_selectionVertexMemory{VK_NULL_HANDLE};
    uint32_t m_selectionVertexCount{0};
    VkBuffer m_lightGizmoVertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_lightGizmoVertexMemory{VK_NULL_HANDLE};
    uint32_t m_lightGizmoVertexCount{0};
    VkSampler m_textureSampler{VK_NULL_HANDLE};
    GpuTexture m_defaultTexture{};
    std::array<VkBuffer, kMaxFramesInFlight> m_uniformBuffers{};
    std::array<VkDeviceMemory, kMaxFramesInFlight> m_uniformMemories{};
    std::array<void*, kMaxFramesInFlight> m_uniformMapped{};
    uint32_t m_frameIndex{0};
    std::vector<VkSemaphore> m_imageAvailable;
    // Must be per-swapchain-image (present may outlive per-frame fences).
    std::vector<VkSemaphore> m_renderFinishedPerImage;
    std::vector<VkFence> m_inFlight;
    std::vector<VkFence> m_imagesInFlight;
    std::unordered_map<std::string, GpuMesh> m_meshCache;
    std::unordered_set<std::string> m_missingMeshes;
    std::unordered_map<std::string, GpuTexture> m_textureCache;
    std::unordered_set<std::string> m_missingTextures;

    void CreateSurface(void* nativeHandle);
    void CreateSwapchain(int width, int height);
    void DestroySwapchain();
    void DestroySwapchainResources();
    void DestroyDeviceResources();
    void DestroyDepthResources();
    void DestroySurface();
    void RecreateRenderer(int width, int height);
    bool TryRecoverSwapchain();

    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateMeshBuffers();
    void CreateLineBuffers();
    void CreateDepthResources();
    void CreateUniformBuffers();
    void CreateDescriptorPoolAndSets();
    void CreateTextureDescriptorPool();
    void CreateTextureResources();
    void CreatePipeline();
    void CreateFramebuffers();

    void CreateCommandPoolAndBuffers();
    void RecordCommandBuffer(uint32_t imageIndex, const std::vector<DrawInstance>& instances);
    void UpdateUniformBuffer(uint32_t frameIndex, const RenderView& view);
    void UpdateSelectionBuffer(const std::vector<DrawInstance>& instances, Core::EntityId selectedId);
    void UpdateLightGizmoBuffer(const RenderView& view);
    void DestroyMeshCache();
    void DestroyTextureCache();

#ifdef __APPLE__
    void UpdateMetalLayerSize(int width, int height);
#endif

    void CreateSyncObjects();
    [[nodiscard]] std::vector<DrawInstance> InstancesFromView(const RenderView& view, float timeSeconds) const;

    [[nodiscard]] const GpuMesh* ResolveMesh(const std::string& assetId);
    [[nodiscard]] const GpuTexture* ResolveTexture(const std::string& assetId);
    GpuTexture CreateTextureFromPixels(const unsigned char* pixels, uint32_t width, uint32_t height);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    [[nodiscard]] std::string ShaderPath(const char* filename) const;
    [[nodiscard]] std::vector<char> ReadFileBinary(const std::string& path) const;
    [[nodiscard]] VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
};
} // namespace Aetherion::Rendering
