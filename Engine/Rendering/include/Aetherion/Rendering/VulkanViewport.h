#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Aetherion/Rendering/RenderView.h"

namespace Aetherion::Rendering
{
class VulkanContext;

class VulkanViewport
{
public:
    explicit VulkanViewport(std::shared_ptr<VulkanContext> context);
    ~VulkanViewport();

    VulkanViewport(const VulkanViewport&) = delete;
    VulkanViewport& operator=(const VulkanViewport&) = delete;

    void Initialize(void* nativeHandle, int width, int height);
    void Resize(int width, int height);
    void RenderFrame(float deltaTimeSeconds, const RenderView& view);
    void Shutdown();

    [[nodiscard]] bool IsReady() const noexcept { return m_ready; }

private:
    static constexpr uint32_t kMaxFramesInFlight = 2;
    struct TransformData
    {
        float posX = 0.0f;
        float posY = 0.0f;
        float rotDegZ = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

    std::shared_ptr<VulkanContext> m_context;
    bool m_ready{false};
    float m_timeSeconds{0.0f};
    bool m_waitingForValidExtent{false};

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_swapchainFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent{};

    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkRenderPass m_renderPass{VK_NULL_HANDLE};

    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    std::array<VkDescriptorSet, kMaxFramesInFlight> m_descriptorSets{};

    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};

    std::vector<VkFramebuffer> m_framebuffers;

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vertexMemory{VK_NULL_HANDLE};
    VkBuffer m_indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_indexMemory{VK_NULL_HANDLE};
    std::array<VkBuffer, kMaxFramesInFlight> m_uniformBuffers{};
    std::array<VkDeviceMemory, kMaxFramesInFlight> m_uniformMemories{};
    std::array<void*, kMaxFramesInFlight> m_uniformMapped{};
    uint32_t m_frameIndex{0};
    std::vector<VkSemaphore> m_imageAvailable;
    // Must be per-swapchain-image (present may outlive per-frame fences).
    std::vector<VkSemaphore> m_renderFinishedPerImage;
    std::vector<VkFence> m_inFlight;
    std::vector<VkFence> m_imagesInFlight;

    void CreateSurface(void* nativeHandle);
    void CreateSwapchain(int width, int height);
    void DestroySwapchain();

    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateMeshBuffers();
    void CreateUniformBuffers();
    void CreateDescriptorPoolAndSets();
    void CreatePipeline();
    void CreateFramebuffers();

    void CreateCommandPoolAndBuffers();
    void RecordCommandBuffer(uint32_t imageIndex, bool drawGeometry);
    void UpdateUniformBuffer(uint32_t frameIndex, const TransformData& transform);

    void CreateSyncObjects();
    [[nodiscard]] TransformData TransformFromView(const RenderView& view, float timeSeconds) const;

    [[nodiscard]] std::string ShaderPath(const char* filename) const;
    [[nodiscard]] std::vector<char> ReadFileBinary(const std::string& path) const;
    [[nodiscard]] VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
};
} // namespace Aetherion::Rendering
