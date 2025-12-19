#include "Aetherion/Rendering/VulkanContext.h"

#ifdef __APPLE__
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_macos.h>
#include <vulkan/vulkan_metal.h>
#include <vulkan/vulkan_beta.h>
#endif

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <set>
#include <string_view>
#include <utility>

namespace
{
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
    // MoltenVK requires portability subset on macOS.
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
};
}

namespace Aetherion::Rendering
{
void VulkanContext::SetLogCallback(std::function<void(LogSeverity, const std::string&)> callback)
{
    m_logCallback = std::move(callback);
}

void VulkanContext::Log(LogSeverity severity, const std::string& message) const
{
    const bool forward = m_enableLogging || severity != LogSeverity::Info;
    if (forward && m_logCallback)
    {
        m_logCallback(severity, message);
    }

    const bool alwaysPrint = severity != LogSeverity::Info;
    if (!m_enableLogging && !alwaysPrint)
    {
        return;
    }

    auto& stream = (severity == LogSeverity::Error) ? std::cerr : std::cout;
    stream << message << std::endl;
}

VulkanContext::VulkanContext() = default;

VulkanContext::~VulkanContext()
{
    Shutdown();
}

void VulkanContext::Initialize(bool enableValidation, bool enableLogging)
{
    if (m_initialized)
    {
        return;
    }

    m_enableValidation = enableValidation;
    m_enableLogging = enableLogging;

    if (m_enableValidation && !CheckValidationLayerSupport())
    {
        Log(LogSeverity::Warning,
            "Vulkan validation layer VK_LAYER_KHRONOS_validation not available; continuing without validation.");
        m_enableValidation = false;
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
    m_logCallback = nullptr;
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

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

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

#ifdef __APPLE__
    // MoltenVK (Vulkan-on-Metal) uses a portability subset and a macOS surface extension.
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    // Keep MVK macOS surface as an additional option (some toolkits still expose an NSView handle).
    extensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
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

    const char* preferredGpuEnv = std::getenv("AETHERION_PREFERRED_GPU");
    const std::string_view preferredGpu = preferredGpuEnv ? std::string_view(preferredGpuEnv) : std::string_view{};
    const bool hasPreferredGpu = !preferredGpu.empty();

    bool foundSurfaceButNoSwapchain = false;

    auto trySelectDevice = [&](VkPhysicalDevice device) -> bool {
        if (!CheckDeviceExtensionSupport(device))
        {
            return false;
        }

        auto indices = FindQueueFamilies(device, surface);
        if (!indices.IsComplete())
        {
            return false;
        }

        bool swapchainAdequate = true;
        if (surface != VK_NULL_HANDLE)
        {
            auto support = QuerySwapchainSupport(device, surface);
            swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
            if (!swapchainAdequate)
            {
                foundSurfaceButNoSwapchain = true;
            }
        }

            if (swapchainAdequate)
            {
                m_physicalDevice = device;
                m_queueFamilyIndices = indices;
                m_graphicsQueueFamilyIndex = indices.graphicsFamily.value();
                m_presentQueueFamilyIndex = indices.presentFamily.value();

                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
                Log(LogSeverity::Info,
                    "VulkanContext: selected GPU '" + std::string(props.deviceName) + "' (graphics queue " +
                        std::to_string(m_graphicsQueueFamilyIndex) + ", present queue " +
                        std::to_string(m_presentQueueFamilyIndex) + ")");
                return true;
            }

        return false;
    };

    auto matchesPreference = [&](VkPhysicalDevice device) -> bool {
        if (!hasPreferredGpu)
        {
            return true;
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);
        const std::string_view name(props.deviceName);

        auto toLower = [](char c) {
            if (c >= 'A' && c <= 'Z')
            {
                return static_cast<char>(c - 'A' + 'a');
            }
            return c;
        };

        // Simple ASCII case-insensitive substring match.
        for (size_t start = 0; start < name.size(); ++start)
        {
            size_t i = 0;
            while (start + i < name.size() && i < preferredGpu.size() &&
                   toLower(name[start + i]) == toLower(preferredGpu[i]))
            {
                ++i;
            }
            if (i == preferredGpu.size())
            {
                return true;
            }
        }
        return false;
    };

    if (hasPreferredGpu)
    {
        bool foundPreferred = false;
        for (auto device : devices)
        {
            if (!matchesPreference(device))
            {
                continue;
            }
            foundPreferred = true;
            if (trySelectDevice(device))
            {
                return;
            }
        }

            if (m_enableLogging)
            {
                if (!foundPreferred)
                {
                    Log(LogSeverity::Warning,
                        "VulkanContext: preferred GPU '" + std::string(preferredGpu) +
                            "' not found; falling back to any compatible adapter");
                }
                else
                {
                    Log(LogSeverity::Warning,
                        "VulkanContext: preferred GPU '" + std::string(preferredGpu) +
                            "' found but not compatible; falling back to any compatible adapter");
                }
            }
    }

    for (auto device : devices)
    {
        if (trySelectDevice(device))
        {
            return;
        }
    }

    if (foundSurfaceButNoSwapchain)
    {
        throw std::runtime_error("Found GPU(s) without adequate swapchain support for this surface");
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

    if (deviceChanged || queuesChanged)
    {
        Log(LogSeverity::Warning,
            "VulkanContext: adapter/queue change detected (graphics " + std::to_string(previousGraphics) +
                " -> " + std::to_string(m_graphicsQueueFamilyIndex) + ", present " +
                std::to_string(previousPresent) + " -> " + std::to_string(m_presentQueueFamilyIndex) + ")");
    }

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
    auto* context = reinterpret_cast<VulkanContext*>(pUserData);

    const char* severityStr = "";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severityStr = "[ERROR]";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severityStr = "[WARN]";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severityStr = "[INFO]";
    else
        severityStr = "[VERBOSE]";

    const LogSeverity logSeverity =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            ? LogSeverity::Error
            : ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? LogSeverity::Warning
                                                                            : LogSeverity::Info);

    const std::string message = std::string("Vulkan ") + severityStr + ": " + pCallbackData->pMessage;

    if (context)
    {
        context->Log(logSeverity, message);
    }
    else
    {
        std::cerr << message << std::endl;
    }
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
    createInfo.pUserData = this;

    if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) !=
        VK_SUCCESS)
    {
        Log(LogSeverity::Warning, "Failed to set up debug messenger");
    }
}

void VulkanContext::LogDeviceInfo() const
{
    if (!m_enableLogging)
    {
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);

    std::string info = "\n=== Vulkan Device Info ===\n";
    info += "Device: " + std::string(props.deviceName) + "\n";
    info += "API Version: " + std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
            std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." +
            std::to_string(VK_VERSION_PATCH(props.apiVersion)) + "\n";
    info += "Driver Version: " + std::to_string(props.driverVersion) + "\n";
    info += "Vendor ID: " + std::to_string(props.vendorID) + "\n";
    info += "========================\n";

    Log(LogSeverity::Info, info);
}
} // namespace Aetherion::Rendering
