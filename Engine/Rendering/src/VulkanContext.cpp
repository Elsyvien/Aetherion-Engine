#include "Aetherion/Rendering/VulkanContext.h"

#include <iostream>
#include <stdexcept>
#include <set>

namespace
{
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
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
    SetupDebugMessenger();
    PickPhysicalDevice(VK_NULL_HANDLE);
    CreateLogicalDevice();
    LogDeviceInfo();

    m_initialized = true;
}

void VulkanContext::Shutdown()
{
    if (m_debugMessenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE)
    {
        auto vkDestroyDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (vkDestroyDebugUtilsMessengerEXT)
        {
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }

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
    m_presentQueue = VK_NULL_HANDLE;
    m_queueFamilyIndices = {};
    m_graphicsQueueFamilyIndex = 0;
    m_presentQueueFamilyIndex = 0;
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

    // Required for creating a presentation surface + swapchain.
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef _WIN32
    extensions.push_back("VK_KHR_win32_surface");
#endif

    if (m_enableValidation)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VulkanContext::QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices{};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        if (surface != VK_NULL_HANDLE)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = i;
            }
        }
    }

    if (!indices.presentFamily.has_value() && indices.graphicsFamily.has_value())
    {
        // Without a surface we fall back to the graphics queue for presentation.
        indices.presentFamily = indices.graphicsFamily;
    }

    return indices;
}

bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

    for (const auto* required : kDeviceExtensions)
    {
        const std::string requiredName(required);
        bool found = false;
        for (const auto& ext : available)
        {
            if (requiredName == ext.extensionName)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }

    return true;
}

void VulkanContext::PickPhysicalDevice(VkSurfaceKHR surface)
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
        if (!CheckDeviceExtensionSupport(device))
        {
            continue;
        }

        auto indices = FindQueueFamilies(device, surface);
        if (!indices.IsComplete())
        {
            continue;
        }

        bool swapchainAdequate = true;
        if (surface != VK_NULL_HANDLE)
        {
            auto support = QuerySwapchainSupport(device, surface);
            swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
        }

        if (swapchainAdequate)
        {
            m_physicalDevice = device;
            m_queueFamilyIndices = indices;
            m_graphicsQueueFamilyIndex = indices.graphicsFamily.value();
            m_presentQueueFamilyIndex = indices.presentFamily.value();
            return;
        }
    }

    throw std::runtime_error("No suitable GPU with graphics/present/swapchain support found");
}

void VulkanContext::CreateLogicalDevice()
{
    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("VulkanContext: physical device not selected before device creation");
    }

    const float priority = 1.0f;
    std::set<uint32_t> uniqueQueueFamilies = {
        m_graphicsQueueFamilyIndex,
        m_presentQueueFamilyIndex,
    };

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueQueueFamilies.size());
    for (uint32_t family : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan logical device");
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamilyIndex, 0, &m_presentQueue);
}

VulkanContext::SwapchainSupportDetails VulkanContext::QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
    SwapchainSupportDetails details{};

    if (device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    {
        return details;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VulkanContext::SwapchainSupportDetails VulkanContext::QuerySwapchainSupport(VkSurfaceKHR surface) const
{
    return QuerySwapchainSupport(m_physicalDevice, surface);
}

void VulkanContext::EnsureSurfaceCompatibility(VkSurfaceKHR surface)
{
    if (surface == VK_NULL_HANDLE)
    {
        throw std::runtime_error("EnsureSurfaceCompatibility called with null surface");
    }

    const VkPhysicalDevice previousDevice = m_physicalDevice;
    const uint32_t previousGraphics = m_graphicsQueueFamilyIndex;
    const uint32_t previousPresent = m_presentQueueFamilyIndex;

    PickPhysicalDevice(surface);

    const bool deviceChanged = previousDevice != m_physicalDevice;
    const bool queuesChanged = (previousGraphics != m_graphicsQueueFamilyIndex) ||
                               (previousPresent != m_presentQueueFamilyIndex);

    if (m_device == VK_NULL_HANDLE || deviceChanged || queuesChanged)
    {
        if (m_device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
            m_graphicsQueue = VK_NULL_HANDLE;
            m_presentQueue = VK_NULL_HANDLE;
        }

        CreateLogicalDevice();
        LogDeviceInfo();
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    const char* severityStr = "";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severityStr = "[ERROR]";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severityStr = "[WARN]";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severityStr = "[INFO]";
    else
        severityStr = "[VERBOSE]";

    std::cerr << "Vulkan " << severityStr << ": " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void VulkanContext::SetupDebugMessenger()
{
    if (!m_enableValidation)
    {
        return;
    }

    auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!vkCreateDebugUtilsMessengerEXT)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) !=
        VK_SUCCESS)
    {
        std::cerr << "Failed to set up debug messenger" << std::endl;
    }
}

void VulkanContext::LogDeviceInfo() const
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);

    std::cout << "\n=== Vulkan Device Info ===" << std::endl;
    std::cout << "Device: " << props.deviceName << std::endl;
    std::cout << "API Version: " << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "." << VK_VERSION_PATCH(props.apiVersion)
              << std::endl;
    std::cout << "Driver Version: " << props.driverVersion << std::endl;
    std::cout << "Vendor ID: " << props.vendorID << std::endl;
    std::cout << "========================\n" << std::endl;
}
} // namespace Aetherion::Rendering
