#include "Aetherion/Rendering/VulkanContext.h"

#include <stdexcept>

namespace
{
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
}

namespace Aetherion::Rendering
{
VulkanContext::VulkanContext() = default;

VulkanContext::~VulkanContext()
{
    Shutdown();
}

void VulkanContext::Initialize(bool enableValidation)
{
    if (m_initialized)
    {
        return;
    }

    m_enableValidation = enableValidation;

    if (m_enableValidation && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("Validation layer VK_LAYER_KHRONOS_validation not available");
    }

    CreateInstance();
    PickPhysicalDevice();
    CreateLogicalDevice();

    m_initialized = true;
}

void VulkanContext::Shutdown()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_initialized = false;
}

void VulkanContext::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Aetherion";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Aetherion";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    auto layers = GetRequiredInstanceLayers();
    auto extensions = GetRequiredInstanceExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

bool VulkanContext::CheckValidationLayerSupport() const
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    for (const auto& layer : available)
    {
        if (std::string(layer.layerName) == kValidationLayer)
        {
            return true;
        }
    }
    return false;
}

std::vector<const char*> VulkanContext::GetRequiredInstanceLayers() const
{
    std::vector<const char*> layers;
    if (m_enableValidation)
    {
        layers.push_back(kValidationLayer);
    }
    return layers;
}

std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions() const
{
    std::vector<const char*> extensions;
    // No surface requested yet; add debug utils when validation is enabled.
    if (m_enableValidation)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

std::optional<uint32_t> VulkanContext::FindGraphicsQueueFamily(VkPhysicalDevice device) const
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return i;
        }
    }
    return std::nullopt;
}

void VulkanContext::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (auto device : devices)
    {
        auto queueFamily = FindGraphicsQueueFamily(device);
        if (queueFamily.has_value())
        {
            m_physicalDevice = device;
            m_graphicsQueueFamilyIndex = queueFamily.value();
            return;
        }
    }

    throw std::runtime_error("No suitable GPU with graphics queue found");
}

void VulkanContext::CreateLogicalDevice()
{
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.pEnabledFeatures = &features;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan logical device");
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
}
} // namespace Aetherion::Rendering
