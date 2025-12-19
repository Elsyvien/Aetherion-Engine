#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace Aetherion::Rendering
{
enum class LogSeverity
{
    Info,
    Warning,
    Error,
};

class VulkanContext
{
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void Initialize(bool enableValidation, bool enableLogging);
    void Shutdown();

    void SetLogCallback(std::function<void(LogSeverity, const std::string&)> callback);
    void Log(LogSeverity severity, const std::string& message) const;
    void SetLoggingEnabled(bool enabled) noexcept { m_enableLogging = enabled; }

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool IsValidationEnabled() const noexcept { return m_enableValidation; }
    [[nodiscard]] bool IsLoggingEnabled() const noexcept { return m_enableLogging; }

    [[nodiscard]] VkInstance GetInstance() const noexcept { return m_instance; }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept { return m_physicalDevice; }
    [[nodiscard]] VkDevice GetDevice() const noexcept { return m_device; }
    [[nodiscard]] VkQueue GetGraphicsQueue() const noexcept { return m_graphicsQueue; }
    [[nodiscard]] uint32_t GetGraphicsQueueFamilyIndex() const noexcept { return m_graphicsQueueFamilyIndex; }
    [[nodiscard]] VkQueue GetPresentQueue() const noexcept { return m_presentQueue; }
    [[nodiscard]] uint32_t GetPresentQueueFamilyIndex() const noexcept { return m_presentQueueFamilyIndex; }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool IsComplete() const noexcept
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    void EnsureSurfaceCompatibility(VkSurfaceKHR surface);
    [[nodiscard]] SwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR surface) const;
    [[nodiscard]] QueueFamilyIndices GetQueueFamilyIndices() const noexcept { return m_queueFamilyIndices; }

    void LogDeviceInfo() const;

private:
    bool m_initialized{false};
    bool m_enableValidation{false};
    bool m_enableLogging{true};
    std::function<void(LogSeverity, const std::string&)> m_logCallback;
    VkInstance m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    uint32_t m_graphicsQueueFamilyIndex{0};
    uint32_t m_presentQueueFamilyIndex{0};
    QueueFamilyIndices m_queueFamilyIndices{};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice(VkSurfaceKHR surface);
    void CreateLogicalDevice();

    [[nodiscard]] bool CheckValidationLayerSupport() const;
    [[nodiscard]] bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
    [[nodiscard]] std::vector<const char*> GetRequiredInstanceLayers() const;
    [[nodiscard]] std::vector<const char*> GetRequiredInstanceExtensions() const;
    [[nodiscard]] QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    [[nodiscard]] SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;
};
} // namespace Aetherion::Rendering
