#pragma once

#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace Aetherion::Rendering
{
class VulkanContext
{
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void Initialize(bool enableValidation);
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] VkInstance GetInstance() const noexcept { return m_instance; }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept { return m_physicalDevice; }
    [[nodiscard]] VkDevice GetDevice() const noexcept { return m_device; }
    [[nodiscard]] VkQueue GetGraphicsQueue() const noexcept { return m_graphicsQueue; }
    [[nodiscard]] uint32_t GetGraphicsQueueFamilyIndex() const noexcept { return m_graphicsQueueFamilyIndex; }

    void LogDeviceInfo() const;

private:
    bool m_initialized{false};
    bool m_enableValidation{false};
    VkInstance m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    uint32_t m_graphicsQueueFamilyIndex{0};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    [[nodiscard]] bool CheckValidationLayerSupport() const;
    [[nodiscard]] std::vector<const char*> GetRequiredInstanceLayers() const;
    [[nodiscard]] std::vector<const char*> GetRequiredInstanceExtensions() const;
    [[nodiscard]] std::optional<uint32_t> FindGraphicsQueueFamily(VkPhysicalDevice device) const;
};
} // namespace Aetherion::Rendering
