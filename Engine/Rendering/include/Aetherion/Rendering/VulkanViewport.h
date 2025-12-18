#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

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
    void RenderFrame();
    void Shutdown();

    [[nodiscard]] bool IsReady() const noexcept { return m_ready; }

private:
    std::shared_ptr<VulkanContext> m_context;
    bool m_ready{false};

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_swapchainFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent{};

    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};

    std::vector<VkFramebuffer> m_framebuffers;

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers;

    static constexpr uint32_t kMaxFramesInFlight = 2;
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
    void CreatePipeline();
    void CreateFramebuffers();

    void CreateCommandPoolAndBuffers();
    void RecordCommandBuffer(uint32_t imageIndex);

    void CreateSyncObjects();

    [[nodiscard]] std::string ShaderPath(const char* filename) const;
    [[nodiscard]] std::vector<char> ReadFileBinary(const std::string& path) const;
    [[nodiscard]] VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
};
} // namespace Aetherion::Rendering
