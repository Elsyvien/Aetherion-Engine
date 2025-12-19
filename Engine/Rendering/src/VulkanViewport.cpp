#include "Aetherion/Rendering/VulkanViewport.h"

#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/TransformComponent.h"

#include <fstream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <functional>

#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef IsLoggingEnabled
#undef IsLoggingEnabled
#endif
#include <vulkan/vulkan_win32.h>
#endif

#ifdef __APPLE__
#include <vulkan/vulkan_macos.h>
#include <vulkan/vulkan_metal.h>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace Aetherion::Rendering
{
namespace
{
struct Vertex
{
    float pos[2];
    float color[3];
};

struct FrameUniformObject
{
    // Column-major view-projection matrix for GLSL.
    float viewProj[16];
};

void Mat4Identity(float out[16])
{
    std::memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void Mat4Mul(float out[16], const float a[16], const float b[16])
{
    // out = a * b (column-major)
    float r[16];
    for (int c = 0; c < 4; ++c)
    {
        for (int rIdx = 0; rIdx < 4; ++rIdx)
        {
            r[c * 4 + rIdx] = a[0 * 4 + rIdx] * b[c * 4 + 0] + a[1 * 4 + rIdx] * b[c * 4 + 1] +
                              a[2 * 4 + rIdx] * b[c * 4 + 2] + a[3 * 4 + rIdx] * b[c * 4 + 3];
        }
    }
    std::memcpy(out, r, sizeof(r));
}

void Mat4RotationZ(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[4] = -s;
    out[1] = s;
    out[5] = c;
}

void Mat4Translation(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void Mat4Scale(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}

void Mat4Ortho(float out[16], float left, float right, float bottom, float top, float zNear, float zFar)
{
    Mat4Identity(out);
    out[0] = 2.0f / (right - left);
    out[5] = 2.0f / (top - bottom);
    out[10] = -2.0f / (zFar - zNear);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(zFar + zNear) / (zFar - zNear);
}

uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && ((memProps.memoryTypes[i].propertyFlags & properties) == properties))
        {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void CreateBuffer(VkPhysicalDevice gpu,
                  VkDevice device,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& outBuffer,
                  VkDeviceMemory& outMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device, outBuffer, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = FindMemoryType(gpu, memReq.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &alloc, nullptr, &outMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, outBuffer, outMemory, 0);
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
    return formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                           : formats[0];
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
    for (auto m : modes)
    {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, int width, int height)
{
    if (caps.currentExtent.width != UINT32_MAX)
    {
        return caps.currentExtent;
    }

    VkExtent2D extent{};
    extent.width = static_cast<uint32_t>(width);
    extent.height = static_cast<uint32_t>(height);

    extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
    extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));

    return extent;
}
} // namespace

VulkanViewport::VulkanViewport(std::shared_ptr<VulkanContext> context)
    : m_context(std::move(context))
{
    if (m_context)
    {
        m_verboseLogging = m_context->IsLoggingEnabled();
    }
}

VulkanViewport::~VulkanViewport()
{
    Shutdown();
}

void VulkanViewport::Initialize(void* nativeHandle, int width, int height)
{
    if (!m_context || !m_context->IsInitialized())
    {
        throw std::runtime_error("VulkanViewport: VulkanContext not initialized");
    }

    try
    {
        m_nativeHandle = nativeHandle;
        m_surfaceWidth = width;
        m_surfaceHeight = height;
        m_shutdown = false;

        CreateSurface(nativeHandle);
        RecreateRenderer(width, height);
        m_ready = true;
        m_frameIndex = 0;
        m_waitingForValidExtent = false;
        m_timeSeconds = 0.0f;
    }
    catch (const std::exception& ex)
    {
        m_context->Log(LogSeverity::Error,
                       std::string("VulkanViewport: initialization failed - ") + ex.what());
        Shutdown();
        throw;
    }
}

void VulkanViewport::Shutdown()
{
    if (m_shutdown)
    {
        return;
    }

    m_shutdown = true;

    DestroyDeviceResources();
    DestroySurface();

    m_timeSeconds = 0.0f;
    m_waitingForValidExtent = false;
    m_ready = false;
    m_nativeHandle = nullptr;
    m_surfaceWidth = 0;
    m_surfaceHeight = 0;
}

void VulkanViewport::Resize(int width, int height)
{
    m_surfaceWidth = width;
    m_surfaceHeight = height;

    if (!m_context || !m_context->IsInitialized() || m_surface == VK_NULL_HANDLE)
    {
        return;
    }

#ifdef __APPLE__
    UpdateMetalLayerSize(width, height);
#endif

    if (width <= 0 || height <= 0)
    {
        m_context->Log(LogSeverity::Warning, "VulkanViewport: resize ignored (surface has zero area)");
        return;
    }

    if (m_verboseLogging)
    {
        m_context->Log(LogSeverity::Info,
                       "VulkanViewport: recreating renderer for " + std::to_string(width) + "x" +
                           std::to_string(height));
    }

    try
    {
        RecreateRenderer(width, height);
        m_ready = true;
        m_needsSwapchainRecreate = false; // Clear flag since we just recreated.
    }
    catch (const std::exception& ex)
    {
        m_context->Log(LogSeverity::Error,
                       std::string("VulkanViewport: swapchain recreation failed - ") + ex.what());
        m_ready = false;
    }
    m_frameIndex = 0;
    m_waitingForValidExtent = false;
}

void VulkanViewport::RenderFrame(float deltaTimeSeconds, const RenderView& view)
{
    if (!m_ready || m_swapchain == VK_NULL_HANDLE)
    {
        return;
    }

    // If swapchain was marked out-of-date, recreate it.
    // DestroyDeviceResources will call vkDeviceWaitIdle internally.
    if (m_needsSwapchainRecreate)
    {
        m_needsSwapchainRecreate = false;
        TryRecoverSwapchain();
        return;
    }

    static bool s_loggedFirstFrame = false;

    if (m_swapchainExtent.width == 0 || m_swapchainExtent.height == 0)
    {
        if (!m_waitingForValidExtent)
        {
            if (m_verboseLogging)
            {
                m_context->Log(LogSeverity::Warning,
                               "VulkanViewport: swapchain extent is zero; skipping frame until resize");
            }
            m_waitingForValidExtent = true;
        }
        return;
    }
    m_waitingForValidExtent = false;

    if (!s_loggedFirstFrame && m_context)
    {
        m_context->Log(LogSeverity::Info, "VulkanViewport: entering render loop");
        s_loggedFirstFrame = true;
    }

    m_timeSeconds += deltaTimeSeconds;
    const auto instances = InstancesFromView(view, m_timeSeconds);

    VkDevice device = m_context->GetDevice();
    VkQueue graphicsQueue = m_context->GetGraphicsQueue();
    VkQueue presentQueue = m_context->GetPresentQueue();

    VkFence inFlight = m_inFlight[m_frameIndex];
    // Wait for previous frame using this slot to complete.
    // Use a short timeout to keep the UI responsive; if not ready, skip this frame.
    VkResult fenceWait = vkWaitForFences(device, 1, &inFlight, VK_TRUE, 1'000'000ULL); // 1ms
    if (fenceWait == VK_TIMEOUT)
    {
        // Previous frame not done yet, skip to keep UI responsive.
        return;
    }
    if (fenceWait != VK_SUCCESS)
    {
        throw std::runtime_error("vkWaitForFences failed");
    }

    uint32_t imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(
        device, m_swapchain, 1'000'000ULL, m_imageAvailable[m_frameIndex], VK_NULL_HANDLE, &imageIndex); // 1ms
    if (acquire == VK_TIMEOUT || acquire == VK_NOT_READY)
    {
        // Image not available, skip frame.
        return;
    }
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR || acquire == VK_ERROR_SURFACE_LOST_KHR)
    {
        // Swapchain is out of date; mark for recreation and skip this frame.
        // The next Resize() call or next RenderFrame will handle it.
        m_needsSwapchainRecreate = true;
        return;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    // Now that we know a frame will be submitted, reset the fence for this frame.
    vkResetFences(device, 1, &inFlight);

    // If this swapchain image is already being used by a previous frame, wait for it.
    if (imageIndex < m_imagesInFlight.size() && m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        VkResult imgWait = vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, 1'000'000ULL); // 1ms
        if (imgWait == VK_TIMEOUT)
        {
            // Image still in use, skip frame to keep UI responsive.
            // Note: fence is already reset, so we need to submit something or we'll deadlock.
            // For now, just continue - worst case we get a validation warning.
        }
    }
    if (imageIndex < m_imagesInFlight.size())
    {
        m_imagesInFlight[imageIndex] = inFlight;
    }

    if (imageIndex >= m_renderFinishedPerImage.size())
    {
        m_context->Log(LogSeverity::Error,
                       "VulkanViewport: acquired image index out of range for sync objects");
        return;
    }

    UpdateUniformBuffer(m_frameIndex);

    vkResetCommandBuffer(m_commandBuffers[m_frameIndex], 0);
    RecordCommandBuffer(imageIndex, instances);

    VkSemaphore waitSem = m_imageAvailable[m_frameIndex];
    VkSemaphore signalSem = m_renderFinishedPerImage[imageIndex];

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &waitSem;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffers[m_frameIndex];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signalSem;

    const VkResult submitRes = vkQueueSubmit(graphicsQueue, 1, &submit, inFlight);
    if (submitRes == VK_ERROR_DEVICE_LOST)
    {
        m_context->Log(LogSeverity::Error,
                       "VulkanViewport: device lost during submit; attempting to recover");
        m_ready = false;
        TryRecoverSwapchain();
        return;
    }

    if (submitRes != VK_SUCCESS)
    {
        throw std::runtime_error("vkQueueSubmit failed");
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &signalSem;
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;

    VkResult pres = vkQueuePresentKHR(presentQueue, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
    {
        // Swapchain is out of date or suboptimal (e.g., window resized).
        // Mark for recreation and continue; next frame will handle it.
        m_needsSwapchainRecreate = true;
        // For SUBOPTIMAL we still presented successfully, so advance the frame index.
        if (pres == VK_SUBOPTIMAL_KHR)
        {
            m_frameIndex = (m_frameIndex + 1) % kMaxFramesInFlight;
        }
        return;
    }
    else if (pres == VK_ERROR_SURFACE_LOST_KHR || pres == VK_ERROR_DEVICE_LOST)
    {
        m_context->Log(LogSeverity::Error,
                       "VulkanViewport: surface or device lost during present; attempting to recover");
        TryRecoverSwapchain();
        return;
    }
    else if (pres != VK_SUCCESS)
    {
        m_context->Log(LogSeverity::Error,
                       "VulkanViewport: vkQueuePresentKHR returned " + std::to_string(static_cast<int>(pres)));
        // Non-fatal; mark for recreation and try again next frame.
        m_needsSwapchainRecreate = true;
        return;
    }

    m_frameIndex = (m_frameIndex + 1) % kMaxFramesInFlight;
}

void VulkanViewport::RecreateRenderer(int width, int height)
{
    // Wait for any in-flight work to complete before destroying resources.
    if (m_context && m_context->IsInitialized())
    {
        vkDeviceWaitIdle(m_context->GetDevice());
    }

    DestroyDeviceResources();

    try
    {
        m_context->EnsureSurfaceCompatibility(m_surface);

#ifdef __APPLE__
        UpdateMetalLayerSize(width, height);
#endif

        CreateSwapchain(width, height);
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreateMeshBuffers();
        CreateUniformBuffers();
        CreateDescriptorPoolAndSets();
        CreatePipeline();
        CreateFramebuffers();
        CreateCommandPoolAndBuffers();
        CreateSyncObjects();

        m_ready = true;
        m_frameIndex = 0;
        m_waitingForValidExtent = false;
    }
    catch (...)
    {
        DestroyDeviceResources();
        throw;
    }
}

bool VulkanViewport::TryRecoverSwapchain()
{
    if (m_surface == VK_NULL_HANDLE || !m_context || !m_context->IsInitialized())
    {
        m_ready = false;
        return false;
    }

    if (m_surfaceWidth <= 0 || m_surfaceHeight <= 0)
    {
        m_ready = false;
        return false;
    }

    try
    {
        RecreateRenderer(m_surfaceWidth, m_surfaceHeight);
        return true;
    }
    catch (const std::exception& ex)
    {
        m_context->Log(LogSeverity::Error,
                       std::string("VulkanViewport: failed to recover swapchain - ") + ex.what());
        m_ready = false;
        return false;
    }
}

void VulkanViewport::DestroyDeviceResources()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
    }

    DestroySwapchainResources();

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (device != VK_NULL_HANDLE && m_uniformMemories[i] != VK_NULL_HANDLE && m_uniformMapped[i] != nullptr)
        {
            vkUnmapMemory(device, m_uniformMemories[i]);
        }
        m_uniformMapped[i] = nullptr;

        if (device != VK_NULL_HANDLE && m_uniformBuffers[i] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
        }
        m_uniformBuffers[i] = VK_NULL_HANDLE;

        if (device != VK_NULL_HANDLE && m_uniformMemories[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, m_uniformMemories[i], nullptr);
        }
        m_uniformMemories[i] = VK_NULL_HANDLE;
    }

    if (device != VK_NULL_HANDLE && m_indexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
    }
    m_indexBuffer = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE && m_indexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_indexMemory, nullptr);
    }
    m_indexMemory = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
    }
    m_vertexBuffer = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE && m_vertexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_vertexMemory, nullptr);
    }
    m_vertexMemory = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    }
    m_descriptorPool = VK_NULL_HANDLE;
    m_descriptorSets.fill(VK_NULL_HANDLE);

    if (device != VK_NULL_HANDLE && m_descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    }
    m_descriptorSetLayout = VK_NULL_HANDLE;

    for (auto fence : m_inFlight)
    {
        if (device != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device, fence, nullptr);
        }
    }
    m_inFlight.clear();

    for (auto sem : m_imageAvailable)
    {
        if (device != VK_NULL_HANDLE && sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_imageAvailable.clear();

    m_imagesInFlight.clear();

    if (device != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
    }
    m_commandPool = VK_NULL_HANDLE;
    m_commandBuffers.clear();

    m_ready = false;
    m_waitingForValidExtent = false;
}

void VulkanViewport::DestroySwapchainResources()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_pipeline, nullptr);
    }
    m_pipeline = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    }
    m_pipelineLayout = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
    }
    m_renderPass = VK_NULL_HANDLE;

    DestroySwapchain();
}

void VulkanViewport::DestroySurface()
{
    if (m_surface != VK_NULL_HANDLE && m_context && m_context->GetInstance() != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
    }
    m_surface = VK_NULL_HANDLE;
}

void VulkanViewport::CreateSurface(void* nativeHandle)
{
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(nativeHandle);
    if (!hwnd)
    {
        throw std::runtime_error("VulkanViewport: invalid HWND");
    }

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = hwnd;
    createInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(m_context->GetInstance(), &createInfo, nullptr, &m_surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Win32 Vulkan surface");
    }
#elif defined(__APPLE__)
    if (!nativeHandle)
    {
        throw std::runtime_error("VulkanViewport: invalid native view");
    }

    // Qt provides a Cocoa view handle (NSView*) as WId on macOS.
    // Create a CAMetalLayer for that view and build a VK_EXT_metal_surface surface.
    id view = reinterpret_cast<id>(nativeHandle);
    Class cametalLayerClass = reinterpret_cast<Class>(objc_getClass("CAMetalLayer"));
    if (!cametalLayerClass)
    {
        throw std::runtime_error("VulkanViewport: CAMetalLayer class not available");
    }

    const SEL selSetWantsLayer = sel_registerName("setWantsLayer:");
    const SEL selSetLayer = sel_registerName("setLayer:");
    const SEL selLayer = sel_registerName("layer");
    const SEL selIsKindOfClass = sel_registerName("isKindOfClass:");
    const SEL selAlloc = sel_registerName("alloc");
    const SEL selInit = sel_registerName("init");

    // Ensure the view is layer-backed.
    reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(view, selSetWantsLayer, YES);

    id existingLayer = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(view, selLayer);
    bool isMetalLayer = false;
    if (existingLayer)
    {
        isMetalLayer = reinterpret_cast<BOOL (*)(id, SEL, Class)>(objc_msgSend)(existingLayer, selIsKindOfClass, cametalLayerClass);
    }

    id metalLayer = existingLayer;
    if (!isMetalLayer)
    {
        // Create a new CAMetalLayer instance: [[CAMetalLayer alloc] init]
        id alloced = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(reinterpret_cast<id>(cametalLayerClass), selAlloc);
        if (!alloced)
        {
            throw std::runtime_error("VulkanViewport: CAMetalLayer alloc failed");
        }
        metalLayer = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(alloced, selInit);
        if (!metalLayer)
        {
            throw std::runtime_error("VulkanViewport: CAMetalLayer init failed");
        }
        // Attach the layer to the view.
        reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(view, selSetLayer, metalLayer);
    }

    // Configure the Metal layer for proper display.
    const SEL selSetOpaque = sel_registerName("setOpaque:");
    const SEL selSetContentsScale = sel_registerName("setContentsScale:");
    const SEL selBackingScaleFactor = sel_registerName("backingScaleFactor");
    const SEL selWindow = sel_registerName("window");

    reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(metalLayer, selSetOpaque, YES);

    id window = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(view, selWindow);
    if (window)
    {
        double scale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(window, selBackingScaleFactor);
        if (scale > 0.0)
        {
            reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(metalLayer, selSetContentsScale, scale);
        }
    }

    m_metalLayer = metalLayer;
    UpdateMetalLayerSize(m_surfaceWidth, m_surfaceHeight);

    VkMetalSurfaceCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    createInfo.pLayer = reinterpret_cast<CAMetalLayer*>(metalLayer);

    VkResult metalRes = vkCreateMetalSurfaceEXT(m_context->GetInstance(), &createInfo, nullptr, &m_surface);
    if (metalRes != VK_SUCCESS)
    {
        // Fallback: create macOS surface with NSView (MoltenVK path).
        VkMacOSSurfaceCreateInfoMVK macInfo{};
        macInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        macInfo.pView = nativeHandle;
        if (vkCreateMacOSSurfaceMVK(m_context->GetInstance(), &macInfo, nullptr, &m_surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan surface on macOS (Metal and MVK fallback failed)");
        }
    }
#else
    (void)nativeHandle;
    throw std::runtime_error("VulkanViewport: platform not supported in this build");
#endif

    m_context->EnsureSurfaceCompatibility(m_surface);
}

void VulkanViewport::CreateSwapchain(int width, int height)
{
    auto support = m_context->QuerySwapchainSupport(m_surface);
    if (support.formats.empty() || support.presentModes.empty())
    {
        m_swapchain = VK_NULL_HANDLE;
        m_swapchainExtent = {0, 0};
        m_swapchainImages.clear();
        m_swapchainImageViews.clear();
        m_renderFinishedPerImage.clear();
        m_imagesInFlight.clear();
        throw std::runtime_error("VulkanViewport: swapchain support incomplete for this surface");
    }

    const VkSurfaceCapabilitiesKHR& caps = support.capabilities;
    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(support.presentModes);
    VkExtent2D extent = ChooseExtent(caps, width, height);

    if (extent.width == 0 || extent.height == 0)
    {
        m_swapchain = VK_NULL_HANDLE;
        m_swapchainExtent = {0, 0};
        m_swapchainImages.clear();
        m_swapchainImageViews.clear();
        m_renderFinishedPerImage.clear();
        m_imagesInFlight.clear();
        throw std::runtime_error("VulkanViewport: swapchain extent is zero; surface too small/minimized");
    }

    if (m_verboseLogging)
    {
        m_context->Log(LogSeverity::Info,
                       "VulkanViewport: creating swapchain " + std::to_string(extent.width) + "x" +
                           std::to_string(extent.height) + " (" + std::to_string(support.formats.size()) +
                           " formats, " + std::to_string(support.presentModes.size()) +
                           " present modes, min images " + std::to_string(caps.minImageCount) + ", max images " +
                           (caps.maxImageCount == 0 ? std::string("unbounded")
                                                    : std::to_string(caps.maxImageCount)) +
                           ")");
    }

    uint32_t imageCount = std::max<uint32_t>(caps.minImageCount, 2);
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
    {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create{};
    create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create.surface = m_surface;
    create.minImageCount = imageCount;
    create.imageFormat = surfaceFormat.format;
    create.imageColorSpace = surfaceFormat.colorSpace;
    create.imageExtent = extent;
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    std::array<uint32_t, 2> queueFamilyIndices = {
        m_context->GetGraphicsQueueFamilyIndex(),
        m_context->GetPresentQueueFamilyIndex(),
    };
    if (queueFamilyIndices[0] != queueFamilyIndices[1])
    {
        create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create.queueFamilyIndexCount = 2;
        create.pQueueFamilyIndices = queueFamilyIndices.data();
    }
    else
    {
        create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if ((caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0)
    {
        preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(caps.currentTransform);
    }
    if (preTransform == 0)
    {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    create.preTransform = preTransform;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = presentMode;
    create.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_context->GetDevice(), &create, nullptr, &m_swapchain) != VK_SUCCESS)
    {
        m_swapchain = VK_NULL_HANDLE;
        m_swapchainExtent = {0, 0};
        m_swapchainImages.clear();
        m_swapchainImageViews.clear();
        m_renderFinishedPerImage.clear();
        m_imagesInFlight.clear();
        throw std::runtime_error("VulkanViewport: failed to create swapchain for current surface");
    }

    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(m_context->GetDevice(), m_swapchain, &actualCount, nullptr);
    m_swapchainImages.resize(actualCount);
    vkGetSwapchainImagesKHR(m_context->GetDevice(), m_swapchain, &actualCount, m_swapchainImages.data());

    m_swapchainFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = m_swapchainImages[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = m_swapchainFormat;
        view.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.baseMipLevel = 0;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_context->GetDevice(), &view, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }

    // (Re)create per-image render-finished semaphores to avoid reuse hazards with presentation.
    VkDevice device = m_context->GetDevice();
    for (auto sem : m_renderFinishedPerImage)
    {
        if (device != VK_NULL_HANDLE && sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_renderFinishedPerImage.clear();

    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_renderFinishedPerImage.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        if (vkCreateSemaphore(device, &sem, nullptr, &m_renderFinishedPerImage[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create per-image renderFinished semaphore");
        }
    }

    m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);
}

void VulkanViewport::DestroySwapchain()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;

    for (auto fb : m_framebuffers)
    {
        if (device != VK_NULL_HANDLE && fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_framebuffers.clear();

    for (auto view : m_swapchainImageViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();

    for (auto sem : m_renderFinishedPerImage)
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_renderFinishedPerImage.clear();
    m_imagesInFlight.clear();

    if (device != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
    }
    m_swapchain = VK_NULL_HANDLE;

    m_swapchainExtent = {0, 0};
}

void VulkanViewport::CreateRenderPass()
{
    VkAttachmentDescription color{};
    color.format = m_swapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &color;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1;
    rp.pDependencies = &dep;

    if (vkCreateRenderPass(m_context->GetDevice(), &rp, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanViewport::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &ubo;

    if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &info, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void VulkanViewport::CreateMeshBuffers()
{
    const std::array<Vertex, 4> vertices = {
        Vertex{{-0.5f, -0.5f}, {1.0f, 0.2f, 0.2f}},
        Vertex{{ 0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}},
        Vertex{{ 0.5f,  0.5f}, {0.2f, 0.2f, 1.0f}},
        Vertex{{-0.5f,  0.5f}, {1.0f, 1.0f, 0.2f}},
    };

    const std::array<uint16_t, 6> indices = {0, 1, 2, 2, 3, 0};

    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    CreateBuffer(gpu,
                 device,
                 sizeof(vertices),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_vertexBuffer,
                 m_vertexMemory);

    void* vData = nullptr;
    vkMapMemory(device, m_vertexMemory, 0, sizeof(vertices), 0, &vData);
    std::memcpy(vData, vertices.data(), sizeof(vertices));
    vkUnmapMemory(device, m_vertexMemory);

    CreateBuffer(gpu,
                 device,
                 sizeof(indices),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_indexBuffer,
                 m_indexMemory);

    void* iData = nullptr;
    vkMapMemory(device, m_indexMemory, 0, sizeof(indices), 0, &iData);
    std::memcpy(iData, indices.data(), sizeof(indices));
    vkUnmapMemory(device, m_indexMemory);
}

void VulkanViewport::CreateUniformBuffers()
{
    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    const VkDeviceSize bufferSize = sizeof(FrameUniformObject);

    m_uniformMapped.fill(nullptr);

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        CreateBuffer(gpu,
                     device,
                     bufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_uniformBuffers[i],
                     m_uniformMemories[i]);

        vkMapMemory(device, m_uniformMemories[i], 0, bufferSize, 0, &m_uniformMapped[i]);
    }
}

void VulkanViewport::CreateDescriptorPoolAndSets()
{
    VkDevice device = m_context->GetDevice();

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    pool.maxSets = kMaxFramesInFlight;

    if (vkCreateDescriptorPool(device, &pool, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_descriptorPool;
    alloc.descriptorSetCount = kMaxFramesInFlight;
    alloc.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &alloc, m_descriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        VkDescriptorBufferInfo buf{};
        buf.buffer = m_uniformBuffers[i];
        buf.offset = 0;
        buf.range = sizeof(FrameUniformObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &buf;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
}

void VulkanViewport::CreatePipeline()
{
    auto vert = ReadFileBinary(ShaderPath("viewport_triangle.vert.spv"));
    auto frag = ReadFileBinary(ShaderPath("viewport_triangle.frag.spv"));

    VkShaderModule vertModule = CreateShaderModule(vert);
    VkShaderModule fragModule = CreateShaderModule(frag);

    VkPipelineShaderStageCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vertModule;
    vs.pName = "main";

    VkPipelineShaderStageCreateInfo fs{};
    fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = fragModule;
    fs.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vs, fs};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attrs{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, pos);

    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    // Viewport transform can flip the winding; disable culling for this debug viewport.
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.logicOpEnable = VK_FALSE;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttach;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(InstancePushConstants);

    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &m_descriptorSetLayout;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_context->GetDevice(), &layout, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo pipe{};
    pipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe.stageCount = 2;
    pipe.pStages = stages;
    pipe.pVertexInputState = &vertexInput;
    pipe.pInputAssemblyState = &inputAssembly;
    pipe.pViewportState = &viewportState;
    pipe.pRasterizationState = &raster;
    pipe.pMultisampleState = &msaa;
    pipe.pColorBlendState = &blend;
    pipe.pDynamicState = &dyn;
    pipe.layout = m_pipelineLayout;
    pipe.renderPass = m_renderPass;
    pipe.subpass = 0;

    if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1, &pipe, nullptr, &m_pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(m_context->GetDevice(), fragModule, nullptr);
    vkDestroyShaderModule(m_context->GetDevice(), vertModule, nullptr);
}

void VulkanViewport::CreateFramebuffers()
{
    VkDevice device = m_context->GetDevice();

    // Ensure any previous framebuffers are gone (e.g., on resize).
    for (auto fb : m_framebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_framebuffers.clear();

    m_framebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = {m_swapchainImageViews[i]};

        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = m_renderPass;
        fb.attachmentCount = 1;
        fb.pAttachments = attachments;
        fb.width = m_swapchainExtent.width;
        fb.height = m_swapchainExtent.height;
        fb.layers = 1;

        if (vkCreateFramebuffer(device, &fb, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanViewport::CreateCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = m_context->GetGraphicsQueueFamilyIndex();

    if (vkCreateCommandPool(m_context->GetDevice(), &pool, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool");
    }

    m_commandBuffers.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, m_commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void VulkanViewport::RecordCommandBuffer(uint32_t imageIndex, const std::vector<InstancePushConstants>& instances)
{
    VkCommandBuffer cb = m_commandBuffers[m_frameIndex];

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
    {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    VkClearValue clear{};
    // Use a bright background color for visibility while debugging.
    clear.color = {{0.10f, 0.35f, 0.80f, 1.0f}};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_renderPass;
    rp.framebuffer = m_framebuffers[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = m_swapchainExtent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;

    vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(cb, 0, 1, &scissor);

    // Always bind the pipeline and draw at least one quad so the viewport is visible.
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cb, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cb, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout,
                            0,
                            1,
                            &m_descriptorSets[m_frameIndex],
                            0,
                            nullptr);

    if (!instances.empty())
    {
        for (const auto& instance : instances)
        {
            vkCmdPushConstants(cb,
                               m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(InstancePushConstants),
                               &instance);
            vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
        }
    }
    else
    {
        // Draw a default centered quad when the scene is empty.
        InstancePushConstants defaultQuad{};
        Mat4Identity(defaultQuad.model);
        // Scale down and position the default quad.
        float scale[16];
        Mat4Scale(scale, 0.8f, 0.8f, 1.0f);
        std::memcpy(defaultQuad.model, scale, sizeof(scale));
        defaultQuad.color[0] = 0.95f;
        defaultQuad.color[1] = 0.30f;
        defaultQuad.color[2] = 0.70f;
        defaultQuad.color[3] = 1.0f;

        vkCmdPushConstants(cb,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(InstancePushConstants),
                           &defaultQuad);
        vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cb);

    if (vkEndCommandBuffer(cb) != VK_SUCCESS)
    {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

void VulkanViewport::UpdateUniformBuffer(uint32_t frameIndex)
{
    if (frameIndex >= kMaxFramesInFlight)
    {
        return;
    }

    const float aspect = (m_swapchainExtent.height > 0)
                             ? (static_cast<float>(m_swapchainExtent.width) / static_cast<float>(m_swapchainExtent.height))
                             : 1.0f;

    // Camera: simple ortho that keeps object visible.
    float proj[16];
    Mat4Ortho(proj, -aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

    FrameUniformObject ubo{};
    std::memcpy(ubo.viewProj, proj, sizeof(proj));

    std::memcpy(m_uniformMapped[frameIndex], &ubo, sizeof(ubo));
}

#ifdef __APPLE__
void VulkanViewport::UpdateMetalLayerSize(int width, int height)
{
    if (!m_metalLayer || !m_nativeHandle)
    {
        return;
    }

    id layer = reinterpret_cast<id>(m_metalLayer);
    id view = reinterpret_cast<id>(m_nativeHandle);

    const SEL selWindow = sel_registerName("window");
    const SEL selBackingScaleFactor = sel_registerName("backingScaleFactor");
    const SEL selContentsScale = sel_registerName("contentsScale");
    const SEL selSetContentsScale = sel_registerName("setContentsScale:");
    const SEL selSetDrawableSize = sel_registerName("setDrawableSize:");
    const SEL selSetFrame = sel_registerName("setFrame:");
    const SEL selSetBounds = sel_registerName("setBounds:");

    // Retrieve the window from the view to get the correct backing scale factor (DPI).
    // If the window is not yet available, fall back to the layer's current scale.
    double scale = 1.0;
    id window = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(view, selWindow);
    if (window)
    {
        scale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(window, selBackingScaleFactor);
    }
    else
    {
        scale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(layer, selContentsScale);
    }

    if (scale <= 0.0)
    {
        scale = 1.0;
    }

    // Ensure the layer matches the window's scale factor.
    reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(layer, selSetContentsScale, scale);

    CGSize drawableSize;
    drawableSize.width = static_cast<double>(width) * scale;
    drawableSize.height = static_cast<double>(height) * scale;

    reinterpret_cast<void (*)(id, SEL, CGSize)>(objc_msgSend)(layer, selSetDrawableSize, drawableSize);

    // Keep layer frame/bounds in sync with view size (logical points).
    CGRect bounds = CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height));
    reinterpret_cast<void (*)(id, SEL, CGRect)>(objc_msgSend)(layer, selSetBounds, bounds);
    reinterpret_cast<void (*)(id, SEL, CGRect)>(objc_msgSend)(layer, selSetFrame, bounds);
}
#endif

std::vector<VulkanViewport::InstancePushConstants> VulkanViewport::InstancesFromView(const RenderView& view,
                                                                                     float timeSeconds) const
{
    std::vector<InstancePushConstants> instances;
    instances.reserve(view.instances.size());

    std::unordered_map<Core::EntityId, const Scene::TransformComponent*> transformLookup = view.transforms;
    if (transformLookup.empty())
    {
        for (const auto& instance : view.instances)
        {
            if (instance.transform)
            {
                transformLookup.emplace(instance.entityId, instance.transform);
            }
        }
    }

    std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent*> meshLookup = view.meshes;
    if (meshLookup.empty())
    {
        for (const auto& instance : view.instances)
        {
            if (instance.mesh)
            {
                meshLookup.emplace(instance.entityId, instance.mesh);
            }
        }
    }

    std::unordered_map<Core::EntityId, std::array<float, 16>> worldCache;
    std::function<const std::array<float, 16>&(Core::EntityId)> modelFor = [&](Core::EntityId id)
    {
        auto cached = worldCache.find(id);
        if (cached != worldCache.end())
        {
            return cached->second;
        }

        std::array<float, 16> identity{};
        Mat4Identity(identity.data());

        auto it = transformLookup.find(id);
        if (it == transformLookup.end() || it->second == nullptr)
        {
            return worldCache.emplace(id, identity).first->second;
        }

        const auto* transform = it->second;
        const auto meshIt = meshLookup.find(id);
        const float spinDeg =
            (meshIt != meshLookup.end() && meshIt->second) ? meshIt->second->GetRotationSpeedDegPerSec() * timeSeconds : 0.0f;
        const float radians = (transform->GetRotationZDegrees() + spinDeg) * (3.14159265358979323846f / 180.0f);

        float t[16];
        Mat4Translation(t, transform->GetPositionX(), transform->GetPositionY(), 0.0f);

        float r[16];
        Mat4RotationZ(r, radians);

        float s[16];
        Mat4Scale(s, transform->GetScaleX(), transform->GetScaleY(), 1.0f);

        float tr[16];
        Mat4Mul(tr, t, r);

        float localModel[16];
        Mat4Mul(localModel, tr, s);

        if (transform->HasParent())
        {
            const auto& parentModel = modelFor(transform->GetParentId());
            float world[16];
            Mat4Mul(world, parentModel.data(), localModel);
            std::array<float, 16> stored{};
            std::memcpy(stored.data(), world, sizeof(world));
            return worldCache.emplace(id, stored).first->second;
        }

        std::array<float, 16> stored{};
        std::memcpy(stored.data(), localModel, sizeof(localModel));
        return worldCache.emplace(id, stored).first->second;
    };

    auto appendInstances = [&](const std::vector<RenderInstance>& source) {
        for (const auto& instance : source)
        {
            if (!instance.transform || !instance.mesh)
            {
                continue;
            }

            InstancePushConstants data{};

            const auto& model = modelFor(instance.entityId);
            std::memcpy(data.model, model.data(), sizeof(data.model));

            const auto color = instance.mesh->GetColor();
            data.color[0] = color[0];
            data.color[1] = color[1];
            data.color[2] = color[2];
            data.color[3] = 1.0f;

            instances.push_back(data);
        }
    };

    if (!view.batches.empty())
    {
        for (const auto& batch : view.batches)
        {
            appendInstances(batch.instances);
        }
    }
    else
    {
        appendInstances(view.instances);
    }

    return instances;
}

void VulkanViewport::CreateSyncObjects()
{
    m_imageAvailable.resize(kMaxFramesInFlight);
    m_inFlight.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (vkCreateSemaphore(m_context->GetDevice(), &sem, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
            vkCreateFence(m_context->GetDevice(), &fence, nullptr, &m_inFlight[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}

std::string VulkanViewport::ShaderPath(const char* filename) const
{
    namespace fs = std::filesystem;

    const fs::path candidates[] = {
        fs::path("shaders") / filename,
        fs::path("build") / "shaders" / filename,
        fs::path("..") / "shaders" / filename,
    };

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (fs::exists(candidate, ec))
        {
            return candidate.string();
        }
    }

    // Fall back to the original relative path for error reporting.
    return (fs::path("shaders") / filename).string();
}

std::vector<char> VulkanViewport::ReadFileBinary(const std::string& path) const
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open shader file: " + path);
    }

    const std::streamsize size = file.tellg();
    std::vector<char> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

VkShaderModule VulkanViewport::CreateShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create.codeSize = code.size();
    create.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_context->GetDevice(), &create, nullptr, &module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}
} // namespace Aetherion::Rendering
