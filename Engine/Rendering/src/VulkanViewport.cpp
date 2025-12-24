#include "Aetherion/Rendering/VulkanViewport.h"

#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Assets/AssetRegistry.h"
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
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


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
    float pos[3];
    float normal[3];
    float color[4];
    float uv[2];
};

struct FrameUniformObject
{
    // Column-major view-projection matrix for GLSL.
    float viewProj[16];
    float lightDir[4];
    float lightColor[4];
    float ambientColor[4];
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

void Mat4RotationX(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[5] = c;
    out[9] = -s;
    out[6] = s;
    out[10] = c;
}

void Mat4RotationY(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[8] = s;
    out[2] = -s;
    out[10] = c;
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

std::array<float, 3> Mat4TransformPoint(const float m[16], const std::array<float, 3>& p)
{
    return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
            m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
            m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
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

void Vec3Normalize(float v[3])
{
    const float lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (lenSq <= 0.0f)
    {
        return;
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    v[0] *= invLen;
    v[1] *= invLen;
    v[2] *= invLen;
}

void Vec3Cross(float out[3], const float a[3], const float b[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

float Vec3Dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Mat4LookAt(float out[16], const float eye[3], const float center[3], const float up[3])
{
    float f[3] = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
    Vec3Normalize(f);

    float s[3];
    Vec3Cross(s, f, up);
    Vec3Normalize(s);

    float u[3];
    Vec3Cross(u, s, f);

    Mat4Identity(out);
    out[0] = s[0];
    out[4] = s[1];
    out[8] = s[2];
    out[1] = u[0];
    out[5] = u[1];
    out[9] = u[2];
    out[2] = -f[0];
    out[6] = -f[1];
    out[10] = -f[2];
    out[12] = -Vec3Dot(s, eye);
    out[13] = -Vec3Dot(u, eye);
    out[14] = Vec3Dot(f, eye);
}

void Mat4Perspective(float out[16], float fovRadians, float aspect, float zNear, float zFar)
{
    Mat4Identity(out);
    const float f = 1.0f / std::tan(fovRadians * 0.5f);
    out[0] = f / aspect;
    out[5] = -f;
    out[10] = zFar / (zNear - zFar);
    out[11] = -1.0f;
    out[14] = (zFar * zNear) / (zNear - zFar);
    out[15] = 0.0f;
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

bool HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

bool IsSrgbFormat(VkFormat format)
{
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

VkFormat FindDepthFormat(VkPhysicalDevice gpu)
{
    const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (VkFormat format : candidates)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return format;
        }
    }
    throw std::runtime_error("Failed to find suitable depth format");
}

void CreateImage(VkPhysicalDevice gpu,
                 VkDevice device,
                 uint32_t width,
                 uint32_t height,
                 VkFormat format,
                 VkImageTiling tiling,
                 VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& outImage,
                 VkDeviceMemory& outMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &outImage) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(device, outImage, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = FindMemoryType(gpu, memReq.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &alloc, nullptr, &outMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device, outImage, outMemory, 0);
}

VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange.aspectMask = aspect;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &view, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image view");
    }
    return imageView;
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
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
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

VulkanViewport::VulkanViewport(std::shared_ptr<VulkanContext> context,
                               std::shared_ptr<Assets::AssetRegistry> assetRegistry)
    : m_context(std::move(context))
    , m_assetRegistry(std::move(assetRegistry))
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
    UpdateSelectionBuffer(instances, view.selectedEntityId);
    UpdateLightGizmoBuffer(view);

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

    UpdateUniformBuffer(m_frameIndex, view);

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
        CreateDepthResources();
        CreateDescriptorSetLayout();
        CreateCommandPoolAndBuffers();
        CreateMeshBuffers();
        CreateLineBuffers();
        CreateUniformBuffers();
        CreateDescriptorPoolAndSets();
        CreateTextureDescriptorPool();
        CreateTextureResources();
        CreatePipeline();
        CreateFramebuffers();
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
    DestroyMeshCache();
    DestroyTextureCache();

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
    m_defaultIndexCount = 0;

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

    if (device != VK_NULL_HANDLE && m_lineVertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_lineVertexBuffer, nullptr);
    }
    m_lineVertexBuffer = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE && m_lineVertexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_lineVertexMemory, nullptr);
    }
    m_lineVertexMemory = VK_NULL_HANDLE;
    m_lineVertexCount = 0;

    if (device != VK_NULL_HANDLE && m_selectionVertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_selectionVertexBuffer, nullptr);
    }
    m_selectionVertexBuffer = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE && m_selectionVertexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_selectionVertexMemory, nullptr);
    }
    m_selectionVertexMemory = VK_NULL_HANDLE;
    m_selectionVertexCount = 0;

    if (device != VK_NULL_HANDLE && m_lightGizmoVertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_lightGizmoVertexBuffer, nullptr);
    }
    m_lightGizmoVertexBuffer = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE && m_lightGizmoVertexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_lightGizmoVertexMemory, nullptr);
    }
    m_lightGizmoVertexMemory = VK_NULL_HANDLE;
    m_lightGizmoVertexCount = 0;

    if (device != VK_NULL_HANDLE && m_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    }
    m_descriptorPool = VK_NULL_HANDLE;
    m_descriptorSets.fill(VK_NULL_HANDLE);

    if (device != VK_NULL_HANDLE && m_textureDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, m_textureDescriptorPool, nullptr);
    }
    m_textureDescriptorPool = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_descriptorSetLayout != VK_NULL_HANDLE)    
    {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);   
    }
    m_descriptorSetLayout = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_textureDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, m_textureDescriptorSetLayout, nullptr);
    }
    m_textureDescriptorSetLayout = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_textureSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, m_textureSampler, nullptr);
    }
    m_textureSampler = VK_NULL_HANDLE;

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

void VulkanViewport::DestroyDepthResources()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE && m_depthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, m_depthImageView, nullptr);
    }
    m_depthImageView = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_depthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, m_depthImage, nullptr);
    }
    m_depthImage = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_depthMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_depthMemory, nullptr);
    }
    m_depthMemory = VK_NULL_HANDLE;
    m_depthFormat = VK_FORMAT_UNDEFINED;
}

void VulkanViewport::DestroyMeshCache()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;
    for (auto& entry : m_meshCache)
    {
        auto& mesh = entry.second;
        if (device != VK_NULL_HANDLE && mesh.vertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
        }
        if (device != VK_NULL_HANDLE && mesh.vertexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, mesh.vertexMemory, nullptr);
        }
        if (device != VK_NULL_HANDLE && mesh.indexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
        }
        if (device != VK_NULL_HANDLE && mesh.indexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, mesh.indexMemory, nullptr);
        }
        mesh = {};
    }
    m_meshCache.clear();
    m_missingMeshes.clear();
}

void VulkanViewport::DestroyTextureCache()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;

    auto destroyTexture = [device](GpuTexture& texture) {
        if (device != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, texture.view, nullptr);
        }
        if (device != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, texture.image, nullptr);
        }
        if (device != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, texture.memory, nullptr);
        }
        texture = {};
    };

    destroyTexture(m_defaultTexture);

    for (auto& entry : m_textureCache)
    {
        destroyTexture(entry.second);
    }
    m_textureCache.clear();
    m_missingTextures.clear();
}

void VulkanViewport::DestroySwapchainResources()
{
    VkDevice device = (m_context && m_context->IsInitialized()) ? m_context->GetDevice() : VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_pipeline, nullptr);
    }
    m_pipeline = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_linePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_linePipeline, nullptr);
    }
    m_linePipeline = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_overlayPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_overlayPipeline, nullptr);
    }
    m_overlayPipeline = VK_NULL_HANDLE;

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

    DestroyDepthResources();

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
    if (m_depthFormat == VK_FORMAT_UNDEFINED)
    {
        m_depthFormat = FindDepthFormat(m_context->GetPhysicalDevice());
    }

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

    VkAttachmentDescription depth{};
    depth.format = m_depthFormat;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    std::array<VkAttachmentDescription, 2> attachments = {color, depth};
    rp.attachmentCount = static_cast<uint32_t>(attachments.size());
    rp.pAttachments = attachments.data();
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
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &ubo;

    if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &info, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    VkDescriptorSetLayoutBinding sampler{};
    sampler.binding = 0;
    sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler.descriptorCount = 1;
    sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo texInfo{};
    texInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texInfo.bindingCount = 1;
    texInfo.pBindings = &sampler;

    if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &texInfo, nullptr, &m_textureDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture descriptor set layout");
    }
}

void VulkanViewport::CreateMeshBuffers()
{
    const std::array<Vertex, 4> vertices = {
        Vertex{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.2f, 0.2f, 1.0f}, {0.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.2f, 1.0f, 0.2f, 1.0f}, {1.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.2f, 0.2f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        Vertex{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.2f, 1.0f}, {0.0f, 1.0f}},
    };

    const std::array<uint32_t, 6> indices = {0, 1, 2, 2, 3, 0};

    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    const VkDeviceSize vertexSize = sizeof(vertices);
    const VkDeviceSize indexSize = sizeof(indices);

    VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingVertexMemory = VK_NULL_HANDLE;
    VkBuffer stagingIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingIndexMemory = VK_NULL_HANDLE;
    try
    {
        CreateBuffer(gpu,
                     device,
                     vertexSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer,
                     stagingVertexMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexMemory, 0, vertexSize, 0, &vData);
        std::memcpy(vData, vertices.data(), static_cast<size_t>(vertexSize));
        vkUnmapMemory(device, stagingVertexMemory);

        CreateBuffer(gpu,
                     device,
                     vertexSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_vertexBuffer,
                     m_vertexMemory);
        CopyBuffer(stagingVertexBuffer, m_vertexBuffer, vertexSize);

        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexMemory, nullptr);
        stagingVertexBuffer = VK_NULL_HANDLE;
        stagingVertexMemory = VK_NULL_HANDLE;

        CreateBuffer(gpu,
                     device,
                     indexSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer,
                     stagingIndexMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexMemory, 0, indexSize, 0, &iData);
        std::memcpy(iData, indices.data(), static_cast<size_t>(indexSize));
        vkUnmapMemory(device, stagingIndexMemory);

        CreateBuffer(gpu,
                     device,
                     indexSize,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_indexBuffer,
                     m_indexMemory);
        CopyBuffer(stagingIndexBuffer, m_indexBuffer, indexSize);

        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexMemory, nullptr);
        stagingIndexBuffer = VK_NULL_HANDLE;
        stagingIndexMemory = VK_NULL_HANDLE;
    }
    catch (...)
    {
        if (stagingVertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        }
        if (stagingVertexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, stagingVertexMemory, nullptr);
        }
        if (stagingIndexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        }
        if (stagingIndexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, stagingIndexMemory, nullptr);
        }
        throw;
    }
    m_defaultIndexCount = static_cast<uint32_t>(indices.size());
}

void VulkanViewport::CreateLineBuffers()
{
    const float gridHalf = 10.0f;
    const float step = 1.0f;
    const int gridCount = static_cast<int>(gridHalf / step);

    const float normal[3] = {0.0f, 0.0f, 1.0f};
    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>((gridCount * 2 + 1) * 4 + 4));

    for (int i = -gridCount; i <= gridCount; ++i)
    {
        const float t = static_cast<float>(i) * step;
        const float color[4] = {0.35f, 0.35f, 0.35f, 1.0f};

        vertices.push_back(Vertex{{-gridHalf, t, 0.0f}, {normal[0], normal[1], normal[2]}, {color[0], color[1], color[2], color[3]}, {0.0f, 0.0f}});
        vertices.push_back(Vertex{{ gridHalf, t, 0.0f}, {normal[0], normal[1], normal[2]}, {color[0], color[1], color[2], color[3]}, {1.0f, 0.0f}});

        vertices.push_back(Vertex{{t, -gridHalf, 0.0f}, {normal[0], normal[1], normal[2]}, {color[0], color[1], color[2], color[3]}, {0.0f, 0.0f}});
        vertices.push_back(Vertex{{t,  gridHalf, 0.0f}, {normal[0], normal[1], normal[2]}, {color[0], color[1], color[2], color[3]}, {1.0f, 0.0f}});
    }

    // Axis lines (X = red, Y = green).
    vertices.push_back(Vertex{{-gridHalf, 0.0f, 0.0f}, {normal[0], normal[1], normal[2]}, {0.85f, 0.20f, 0.20f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back(Vertex{{ gridHalf, 0.0f, 0.0f}, {normal[0], normal[1], normal[2]}, {0.85f, 0.20f, 0.20f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back(Vertex{{0.0f, -gridHalf, 0.0f}, {normal[0], normal[1], normal[2]}, {0.20f, 0.85f, 0.20f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back(Vertex{{0.0f,  gridHalf, 0.0f}, {normal[0], normal[1], normal[2]}, {0.20f, 0.85f, 0.20f, 1.0f}, {1.0f, 0.0f}});

    m_lineVertexCount = static_cast<uint32_t>(vertices.size());

    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    CreateBuffer(gpu,
                 device,
                 sizeof(Vertex) * vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_lineVertexBuffer,
                 m_lineVertexMemory);

    void* vData = nullptr;
    vkMapMemory(device, m_lineVertexMemory, 0, sizeof(Vertex) * vertices.size(), 0, &vData);
    std::memcpy(vData, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(device, m_lineVertexMemory);

    const size_t maxSelectionVerts = 24;
    CreateBuffer(gpu,
                 device,
                 sizeof(Vertex) * maxSelectionVerts,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_selectionVertexBuffer,
                 m_selectionVertexMemory);
    m_selectionVertexCount = 0;

    // Light gizmo buffer: arrow with direction lines (64 vertices max)
    const size_t maxLightGizmoVerts = 64;
    CreateBuffer(gpu,
                 device,
                 sizeof(Vertex) * maxLightGizmoVerts,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_lightGizmoVertexBuffer,
                 m_lightGizmoVertexMemory);
    m_lightGizmoVertexCount = 0;
}

void VulkanViewport::CreateDepthResources()
{
    if (!m_context || !m_context->IsInitialized())
    {
        return;
    }

    DestroyDepthResources();

    if (m_swapchainExtent.width == 0 || m_swapchainExtent.height == 0)
    {
        return;
    }

    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    if (m_depthFormat == VK_FORMAT_UNDEFINED)
    {
        m_depthFormat = FindDepthFormat(gpu);
    }

    CreateImage(gpu,
                device,
                m_swapchainExtent.width,
                m_swapchainExtent.height,
                m_depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_depthImage,
                m_depthMemory);

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(m_depthFormat))
    {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    m_depthImageView = CreateImageView(device, m_depthImage, m_depthFormat, aspect);
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

void VulkanViewport::CreateTextureDescriptorPool()
{
    VkDevice device = m_context->GetDevice();

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = kMaxTextureDescriptors;

    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    pool.maxSets = kMaxTextureDescriptors;
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &pool, nullptr, &m_textureDescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture descriptor pool");
    }
}

void VulkanViewport::CreateTextureResources()
{
    VkDevice device = m_context->GetDevice();

    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.anisotropyEnable = VK_FALSE;
    sampler.maxAnisotropy = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler.unnormalizedCoordinates = VK_FALSE;
    sampler.compareEnable = VK_FALSE;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &sampler, nullptr, &m_textureSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler");
    }

    const unsigned char white[4] = {255, 255, 255, 255};
    m_defaultTexture = CreateTextureFromPixels(white, 1, 1);
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

    std::array<VkVertexInputAttributeDescription, 4> attrs{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, pos);

    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, normal);

    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(Vertex, color);

    attrs[3].location = 3;
    attrs[3].binding = 0;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(Vertex, uv);

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

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(InstancePushConstants);

    std::array<VkDescriptorSetLayout, 2> setLayouts = {m_descriptorSetLayout, m_textureDescriptorSetLayout};

    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layout.pSetLayouts = setLayouts.data();
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
    pipe.pDepthStencilState = &depth;
    pipe.pDynamicState = &dyn;
    pipe.layout = m_pipelineLayout;
    pipe.renderPass = m_renderPass;
    pipe.subpass = 0;

    if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1, &pipe, nullptr, &m_pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    VkPipelineInputAssemblyStateCreateInfo lineInput = inputAssembly;
    lineInput.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkGraphicsPipelineCreateInfo linePipe = pipe;
    linePipe.pInputAssemblyState = &lineInput;
    if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1, &linePipe, nullptr, &m_linePipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create line pipeline");
    }

    VkPipelineDepthStencilStateCreateInfo overlayDepth = depth;
    overlayDepth.depthTestEnable = VK_FALSE;
    overlayDepth.depthWriteEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo overlayPipe = linePipe;
    overlayPipe.pDepthStencilState = &overlayDepth;
    if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1, &overlayPipe, nullptr, &m_overlayPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create overlay pipeline");
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
        VkImageView attachments[] = {m_swapchainImageViews[i], m_depthImageView};

        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = m_renderPass;
        fb.attachmentCount = 2;
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

void VulkanViewport::RecordCommandBuffer(uint32_t imageIndex, const std::vector<DrawInstance>& instances)
{
    VkCommandBuffer cb = m_commandBuffers[m_frameIndex];

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
    {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    VkClearValue clear[2]{};
    // Use a bright background color for visibility while debugging.
    clear[0].color = {{0.10f, 0.35f, 0.80f, 1.0f}};
    clear[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_renderPass;
    rp.framebuffer = m_framebuffers[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = m_swapchainExtent;
    rp.clearValueCount = 2;
    rp.pClearValues = clear;

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

    const VkDeviceSize offsets[] = {0};
    const VkDescriptorSet uboSet = m_descriptorSets[m_frameIndex];
    VkDescriptorSet boundTextureSet = VK_NULL_HANDLE;
    if (m_defaultTexture.descriptorSet != VK_NULL_HANDLE)
    {
        VkDescriptorSet sets[] = {uboSet, m_defaultTexture.descriptorSet};
        vkCmdBindDescriptorSets(cb,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout,
                                0,
                                2,
                                sets,
                                0,
                                nullptr);
        boundTextureSet = m_defaultTexture.descriptorSet;
    }

    // Some of our shaders (including line/grid) statically use push constants.
    // Ensure they are set at least once before the first draw call.
    InstancePushConstants baseConstants{};
    Mat4Identity(baseConstants.model);
    baseConstants.color[0] = 1.0f;
    baseConstants.color[1] = 1.0f;
    baseConstants.color[2] = 1.0f;
    baseConstants.color[3] = 1.0f;
    vkCmdPushConstants(cb,
                       m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(InstancePushConstants),
                       &baseConstants);

    if (m_linePipeline != VK_NULL_HANDLE && m_lineVertexBuffer != VK_NULL_HANDLE && m_lineVertexCount > 0)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
        vkCmdBindVertexBuffers(cb, 0, 1, &m_lineVertexBuffer, offsets);
        vkCmdDraw(cb, m_lineVertexCount, 1, 0, 0);
    }

    // Always bind the pipeline and draw at least one quad so the viewport is visible.
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    if (!instances.empty())
    {
        bool hasBoundMesh = false;
        VkBuffer boundVertex = VK_NULL_HANDLE;
        VkBuffer boundIndex = VK_NULL_HANDLE;
        for (const auto& instance : instances)
        {
            const GpuTexture* texture = ResolveTexture(instance.textureId);
            VkDescriptorSet textureSet =
                (texture && texture->descriptorSet != VK_NULL_HANDLE) ? texture->descriptorSet
                                                                     : m_defaultTexture.descriptorSet;

            if (textureSet != VK_NULL_HANDLE && textureSet != boundTextureSet)
            {
                VkDescriptorSet sets[] = {uboSet, textureSet};
                vkCmdBindDescriptorSets(cb,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        m_pipelineLayout,
                                        0,
                                        2,
                                        sets,
                                        0,
                                        nullptr);
                boundTextureSet = textureSet;
            }

            const GpuMesh* mesh = ResolveMesh(instance.meshId);
            VkBuffer vertexBuffer = m_vertexBuffer;
            VkBuffer indexBuffer = m_indexBuffer;
            uint32_t indexCount = m_defaultIndexCount;

            if (mesh && mesh->vertexBuffer != VK_NULL_HANDLE && mesh->indexBuffer != VK_NULL_HANDLE &&
                mesh->indexCount > 0)
            {
                vertexBuffer = mesh->vertexBuffer;
                indexBuffer = mesh->indexBuffer;
                indexCount = mesh->indexCount;
            }

            if (!hasBoundMesh || vertexBuffer != boundVertex || indexBuffer != boundIndex)
            {
                vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(cb, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                hasBoundMesh = true;
                boundVertex = vertexBuffer;
                boundIndex = indexBuffer;
            }

            vkCmdPushConstants(cb,
                               m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(InstancePushConstants),
                               &instance.constants);
            vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
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

        if (m_defaultTexture.descriptorSet != VK_NULL_HANDLE)
        {
            VkDescriptorSet sets[] = {uboSet, m_defaultTexture.descriptorSet};
            vkCmdBindDescriptorSets(cb,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_pipelineLayout,
                                    0,
                                    2,
                                    sets,
                                    0,
                                    nullptr);
        }

        vkCmdPushConstants(cb,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(InstancePushConstants),
                           &defaultQuad);
        vkCmdBindVertexBuffers(cb, 0, 1, &m_vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cb, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);       
        vkCmdDrawIndexed(cb, m_defaultIndexCount, 1, 0, 0, 0);
    }

    if (m_overlayPipeline != VK_NULL_HANDLE && m_selectionVertexBuffer != VK_NULL_HANDLE &&
        m_selectionVertexCount > 0)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_overlayPipeline);
        if (m_defaultTexture.descriptorSet != VK_NULL_HANDLE &&
            boundTextureSet != m_defaultTexture.descriptorSet)
        {
            VkDescriptorSet sets[] = {uboSet, m_defaultTexture.descriptorSet};
            vkCmdBindDescriptorSets(cb,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_pipelineLayout,
                                    0,
                                    2,
                                    sets,
                                    0,
                                    nullptr);
            boundTextureSet = m_defaultTexture.descriptorSet;
        }
        vkCmdBindVertexBuffers(cb, 0, 1, &m_selectionVertexBuffer, offsets);
        vkCmdDraw(cb, m_selectionVertexCount, 1, 0, 0);
    }

    // Draw light gizmo (uses line pipeline like grid)
    if (m_linePipeline != VK_NULL_HANDLE && m_lightGizmoVertexBuffer != VK_NULL_HANDLE &&
        m_lightGizmoVertexCount > 0)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
        vkCmdBindVertexBuffers(cb, 0, 1, &m_lightGizmoVertexBuffer, offsets);
        vkCmdDraw(cb, m_lightGizmoVertexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(cb);

    if (vkEndCommandBuffer(cb) != VK_SUCCESS)
    {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

void VulkanViewport::UpdateUniformBuffer(uint32_t frameIndex, const RenderView& view)
{
    if (frameIndex >= kMaxFramesInFlight)
    {
        return;
    }

    const float aspect = (m_swapchainExtent.height > 0)
                             ? (static_cast<float>(m_swapchainExtent.width) / static_cast<float>(m_swapchainExtent.height))
                             : 1.0f;

    float proj[16];
    Mat4Perspective(proj, 60.0f * (3.14159265358979323846f / 180.0f), aspect, 0.1f, 100.0f);

    // Calculate camera position based on orbit parameters
    const float yawRad = m_cameraYawDeg * (3.14159265358979323846f / 180.0f);
    const float pitchRad = m_cameraPitchDeg * (3.14159265358979323846f / 180.0f);
    const float distance = std::max(0.01f, m_cameraDistance * m_cameraZoom);

    // Spherical to Cartesian conversion for orbit camera
    const float eyeX = m_cameraX + distance * std::cos(pitchRad) * std::sin(yawRad);
    const float eyeY = m_cameraY + distance * std::sin(pitchRad);
    const float eyeZ = m_cameraZ + distance * std::cos(pitchRad) * std::cos(yawRad);

    float viewMat[16];
    const float eye[3] = {eyeX, eyeY, eyeZ};
    const float center[3] = {m_cameraX, m_cameraY, m_cameraZ};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    Mat4LookAt(viewMat, eye, center, up);

    float viewProj[16];
    Mat4Mul(viewProj, proj, viewMat);

    FrameUniformObject ubo{};
    std::memcpy(ubo.viewProj, viewProj, sizeof(viewProj));

    const RenderDirectionalLight& light = view.directionalLight;
    float lightDir[3] = {light.direction[0], light.direction[1], light.direction[2]};
    if (light.enabled)
    {
        Vec3Normalize(lightDir);
    }
    else
    {
        lightDir[0] = 0.0f;
        lightDir[1] = -1.0f;
        lightDir[2] = 0.0f;
    }
    ubo.lightDir[0] = lightDir[0];
    ubo.lightDir[1] = lightDir[1];
    ubo.lightDir[2] = lightDir[2];
    ubo.lightDir[3] = IsSrgbFormat(m_swapchainFormat) ? 0.0f : 1.0f;

    const float intensity = light.enabled ? light.intensity : 0.0f;
    ubo.lightColor[0] = light.color[0] * intensity;
    ubo.lightColor[1] = light.color[1] * intensity;
    ubo.lightColor[2] = light.color[2] * intensity;
    ubo.lightColor[3] = 0.0f;

    ubo.ambientColor[0] = light.ambientColor[0];
    ubo.ambientColor[1] = light.ambientColor[1];
    ubo.ambientColor[2] = light.ambientColor[2];
    ubo.ambientColor[3] = 0.0f;

    std::memcpy(m_uniformMapped[frameIndex], &ubo, sizeof(ubo));
}

void VulkanViewport::UpdateSelectionBuffer(const std::vector<DrawInstance>& instances, Core::EntityId selectedId)
{
    m_selectionVertexCount = 0;
    if (selectedId == 0 || !m_assetRegistry || m_selectionVertexMemory == VK_NULL_HANDLE)
    {
        return;
    }

    const DrawInstance* selected = nullptr;
    for (const auto& instance : instances)
    {
        if (instance.entityId == selectedId)
        {
            selected = &instance;
            break;
        }
    }
    if (!selected || selected->meshId.empty())
    {
        return;
    }

    const auto* meshData = m_assetRegistry->LoadMeshData(selected->meshId);
    if (!meshData)
    {
        return;
    }

    const std::array<float, 3> minV = meshData->boundsMin;
    const std::array<float, 3> maxV = meshData->boundsMax;
    const std::array<std::array<float, 3>, 8> corners = {{
        {minV[0], minV[1], minV[2]},
        {maxV[0], minV[1], minV[2]},
        {maxV[0], maxV[1], minV[2]},
        {minV[0], maxV[1], minV[2]},
        {minV[0], minV[1], maxV[2]},
        {maxV[0], minV[1], maxV[2]},
        {maxV[0], maxV[1], maxV[2]},
        {minV[0], maxV[1], maxV[2]},
    }};

    std::array<std::array<float, 3>, 8> worldCorners{};
    for (size_t i = 0; i < corners.size(); ++i)
    {
        worldCorners[i] = Mat4TransformPoint(selected->constants.model, corners[i]);
    }

    const std::array<std::pair<int, int>, 12> edges = {{
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    }};

    std::vector<Vertex> vertices;
    vertices.reserve(edges.size() * 2);
    const float normal[3] = {0.0f, 0.0f, 1.0f};
    const float color[4] = {1.0f, 0.85f, 0.15f, 1.0f};
    for (const auto& edge : edges)
    {
        const auto& a = worldCorners[edge.first];
        const auto& b = worldCorners[edge.second];
        vertices.push_back(Vertex{{a[0], a[1], a[2]}, {normal[0], normal[1], normal[2]}, {color[0], color[1], color[2], color[3]}, {0.0f, 0.0f}});
        vertices.push_back(Vertex{{b[0], b[1], b[2]}, {normal[0], normal[1], normal[2]}, {color[0], color[1], color[2], color[3]}, {0.0f, 0.0f}});
    }

    void* data = nullptr;
    vkMapMemory(m_context->GetDevice(), m_selectionVertexMemory, 0, sizeof(Vertex) * vertices.size(), 0, &data);
    std::memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(m_context->GetDevice(), m_selectionVertexMemory);

    m_selectionVertexCount = static_cast<uint32_t>(vertices.size());
}

void VulkanViewport::UpdateLightGizmoBuffer(const RenderView& view)
{
    m_lightGizmoVertexCount = 0;
    if (!view.directionalLight.enabled || m_lightGizmoVertexMemory == VK_NULL_HANDLE)
    {
        return;
    }

    const float* pos = view.directionalLight.position;
    const float* dir = view.directionalLight.direction;
    const float* col = view.directionalLight.color;
    
    // Normalize direction
    float dirNorm[3] = {dir[0], dir[1], dir[2]};
    const float len = std::sqrt(dirNorm[0] * dirNorm[0] + dirNorm[1] * dirNorm[1] + dirNorm[2] * dirNorm[2]);
    if (len > 0.0001f)
    {
        dirNorm[0] /= len;
        dirNorm[1] /= len;
        dirNorm[2] /= len;
    }

    // Light color for gizmo (slightly brighter for visibility)
    const float gizmoColor[4] = {
        std::min(1.0f, col[0] * 1.2f + 0.2f),
        std::min(1.0f, col[1] * 1.2f + 0.2f),
        std::min(1.0f, col[2] * 0.2f + 0.2f), // Less blue to look more like sun
        1.0f
    };
    const float normal[3] = {0.0f, 1.0f, 0.0f};

    std::vector<Vertex> vertices;
    vertices.reserve(64);

    // Main direction arrow (from light position pointing in direction)
    const float arrowLen = 1.5f;
    const float arrowEnd[3] = {
        pos[0] + dirNorm[0] * arrowLen,
        pos[1] + dirNorm[1] * arrowLen,
        pos[2] + dirNorm[2] * arrowLen
    };

    // Arrow shaft
    vertices.push_back(Vertex{{pos[0], pos[1], pos[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
    vertices.push_back(Vertex{{arrowEnd[0], arrowEnd[1], arrowEnd[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});

    // Find perpendicular vectors for arrowhead
    float up[3] = {0.0f, 1.0f, 0.0f};
    if (std::abs(dirNorm[1]) > 0.99f)
    {
        up[0] = 1.0f;
        up[1] = 0.0f;
        up[2] = 0.0f;
    }
    
    // Cross product: right = dir x up
    float right[3] = {
        dirNorm[1] * up[2] - dirNorm[2] * up[1],
        dirNorm[2] * up[0] - dirNorm[0] * up[2],
        dirNorm[0] * up[1] - dirNorm[1] * up[0]
    };
    float rightLen = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    if (rightLen > 0.0001f)
    {
        right[0] /= rightLen;
        right[1] /= rightLen;
        right[2] /= rightLen;
    }
    
    // Cross product: actualUp = right x dir
    float actualUp[3] = {
        right[1] * dirNorm[2] - right[2] * dirNorm[1],
        right[2] * dirNorm[0] - right[0] * dirNorm[2],
        right[0] * dirNorm[1] - right[1] * dirNorm[0]
    };

    // Arrowhead lines
    const float headSize = 0.25f;
    const float headBack = 0.3f;
    float headBase[3] = {
        arrowEnd[0] - dirNorm[0] * headBack,
        arrowEnd[1] - dirNorm[1] * headBack,
        arrowEnd[2] - dirNorm[2] * headBack
    };

    // Four arrowhead lines
    for (int i = 0; i < 4; ++i)
    {
        float angle = static_cast<float>(i) * 1.5708f; // 90 degrees
        float offsetX = std::cos(angle) * right[0] + std::sin(angle) * actualUp[0];
        float offsetY = std::cos(angle) * right[1] + std::sin(angle) * actualUp[1];
        float offsetZ = std::cos(angle) * right[2] + std::sin(angle) * actualUp[2];
        
        float headPoint[3] = {
            headBase[0] + offsetX * headSize,
            headBase[1] + offsetY * headSize,
            headBase[2] + offsetZ * headSize
        };
        
        vertices.push_back(Vertex{{arrowEnd[0], arrowEnd[1], arrowEnd[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
        vertices.push_back(Vertex{{headPoint[0], headPoint[1], headPoint[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
    }

    // Sun icon: circle around light position
    const float sunRadius = 0.4f;
    const int sunSegments = 12;
    for (int i = 0; i < sunSegments; ++i)
    {
        float angle1 = static_cast<float>(i) * 6.28318f / static_cast<float>(sunSegments);
        float angle2 = static_cast<float>(i + 1) * 6.28318f / static_cast<float>(sunSegments);
        
        float p1[3] = {
            pos[0] + std::cos(angle1) * sunRadius * right[0] + std::sin(angle1) * sunRadius * actualUp[0],
            pos[1] + std::cos(angle1) * sunRadius * right[1] + std::sin(angle1) * sunRadius * actualUp[1],
            pos[2] + std::cos(angle1) * sunRadius * right[2] + std::sin(angle1) * sunRadius * actualUp[2]
        };
        float p2[3] = {
            pos[0] + std::cos(angle2) * sunRadius * right[0] + std::sin(angle2) * sunRadius * actualUp[0],
            pos[1] + std::cos(angle2) * sunRadius * right[1] + std::sin(angle2) * sunRadius * actualUp[1],
            pos[2] + std::cos(angle2) * sunRadius * right[2] + std::sin(angle2) * sunRadius * actualUp[2]
        };
        
        vertices.push_back(Vertex{{p1[0], p1[1], p1[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
        vertices.push_back(Vertex{{p2[0], p2[1], p2[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
    }

    // Sun rays (8 rays emanating from circle)
    const int rayCount = 8;
    const float rayInner = sunRadius;
    const float rayOuter = sunRadius + 0.25f;
    for (int i = 0; i < rayCount; ++i)
    {
        float angle = static_cast<float>(i) * 6.28318f / static_cast<float>(rayCount);
        
        float innerP[3] = {
            pos[0] + std::cos(angle) * rayInner * right[0] + std::sin(angle) * rayInner * actualUp[0],
            pos[1] + std::cos(angle) * rayInner * right[1] + std::sin(angle) * rayInner * actualUp[1],
            pos[2] + std::cos(angle) * rayInner * right[2] + std::sin(angle) * rayInner * actualUp[2]
        };
        float outerP[3] = {
            pos[0] + std::cos(angle) * rayOuter * right[0] + std::sin(angle) * rayOuter * actualUp[0],
            pos[1] + std::cos(angle) * rayOuter * right[1] + std::sin(angle) * rayOuter * actualUp[1],
            pos[2] + std::cos(angle) * rayOuter * right[2] + std::sin(angle) * rayOuter * actualUp[2]
        };
        
        vertices.push_back(Vertex{{innerP[0], innerP[1], innerP[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
        vertices.push_back(Vertex{{outerP[0], outerP[1], outerP[2]}, {normal[0], normal[1], normal[2]}, {gizmoColor[0], gizmoColor[1], gizmoColor[2], gizmoColor[3]}, {0.0f, 0.0f}});
    }

    if (vertices.empty())
    {
        return;
    }

    void* data = nullptr;
    vkMapMemory(m_context->GetDevice(), m_lightGizmoVertexMemory, 0, sizeof(Vertex) * vertices.size(), 0, &data);
    std::memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(m_context->GetDevice(), m_lightGizmoVertexMemory);

    m_lightGizmoVertexCount = static_cast<uint32_t>(vertices.size());
}

void VulkanViewport::SetCameraPosition(float x, float y, float z) noexcept
{
    m_cameraX = x;
    m_cameraY = y;
    m_cameraZ = z;
}

void VulkanViewport::SetCameraRotation(float yawDeg, float pitchDeg) noexcept
{
    m_cameraYawDeg = yawDeg;
    m_cameraPitchDeg = pitchDeg;
}

void VulkanViewport::SetCameraZoom(float zoom) noexcept
{
    m_cameraZoom = zoom;
}

void VulkanViewport::SetCameraDistance(float distance) noexcept
{
    m_cameraDistance = distance;
}

void VulkanViewport::ResetCamera() noexcept
{
    m_cameraX = 0.0f;
    m_cameraY = 0.0f;
    m_cameraZ = 0.0f;
    m_cameraYawDeg = 30.0f;
    m_cameraPitchDeg = 25.0f;
    m_cameraZoom = 1.0f;
    m_cameraDistance = 5.0f;
}

void VulkanViewport::FocusOnBounds(float centerX,
                                   float centerY,
                                   float centerZ,
                                   float radius,
                                   float padding) noexcept
{
    const float clampedRadius = std::max(radius, 0.01f);
    const float fovRad = 60.0f * (3.14159265358979323846f / 180.0f);
    float distance = clampedRadius / std::sin(fovRad * 0.5f);
    distance *= (padding > 0.0f) ? padding : 1.0f;

    m_cameraX = centerX;
    m_cameraY = centerY;
    m_cameraZ = centerZ;
    m_cameraDistance = distance;
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

std::vector<VulkanViewport::DrawInstance> VulkanViewport::InstancesFromView(const RenderView& view,
                                                                             float timeSeconds) const
{
    std::vector<DrawInstance> instances;
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
    auto modelFor = [&](auto&& self, Core::EntityId id) -> const std::array<float, 16>&
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
        const float radiansX = transform->GetRotationXDegrees() * (3.14159265358979323846f / 180.0f);
        const float radiansY = transform->GetRotationYDegrees() * (3.14159265358979323846f / 180.0f);
        const float radiansZ = (transform->GetRotationZDegrees() + spinDeg) * (3.14159265358979323846f / 180.0f);

        float t[16];
        Mat4Translation(t, transform->GetPositionX(), transform->GetPositionY(), transform->GetPositionZ());

        float rx[16];
        float ry[16];
        float rz[16];
        float rzy[16];
        float r[16];
        Mat4RotationX(rx, radiansX);
        Mat4RotationY(ry, radiansY);
        Mat4RotationZ(rz, radiansZ);
        Mat4Mul(rzy, rz, ry);
        Mat4Mul(r, rzy, rx);

        float s[16];
        Mat4Scale(s, transform->GetScaleX(), transform->GetScaleY(), transform->GetScaleZ());

        float tr[16];
        Mat4Mul(tr, t, r);

        float localModel[16];
        Mat4Mul(localModel, tr, s);

        if (transform->HasParent())
        {
            const auto& parentModel = self(self, transform->GetParentId());
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
            const Scene::TransformComponent* transform = instance.transform;
            if (!transform)
            {
                auto it = transformLookup.find(instance.entityId);
                if (it != transformLookup.end())
                {
                    transform = it->second;
                }
            }

            const Scene::MeshRendererComponent* mesh = instance.mesh;
            if (!mesh)
            {
                auto it = meshLookup.find(instance.entityId);
                if (it != meshLookup.end())
                {
                    mesh = it->second;
                }
            }

            if (!transform && !instance.hasModel)
            {
                continue;
            }

            DrawInstance draw{};
            draw.entityId = instance.entityId;

            if (instance.hasModel)
            {
                std::memcpy(draw.constants.model, instance.model, sizeof(draw.constants.model));
            }
            else
            {
                const auto& model = modelFor(modelFor, instance.entityId);
                std::memcpy(draw.constants.model, model.data(), sizeof(draw.constants.model));
            }

            if (mesh)
            {
                const auto color = mesh->GetColor();
                draw.constants.color[0] = color[0];
                draw.constants.color[1] = color[1];
                draw.constants.color[2] = color[2];
                draw.constants.color[3] = 1.0f;
            }
            else
            {
                draw.constants.color[0] = 1.0f;
                draw.constants.color[1] = 1.0f;
                draw.constants.color[2] = 1.0f;
                draw.constants.color[3] = 1.0f;
            }

            draw.meshId = instance.meshAssetId;
            if (draw.meshId.empty() && mesh)
            {
                draw.meshId = mesh->GetMeshAssetId();
            }
            draw.textureId = instance.albedoTextureId;
            if (draw.textureId.empty() && mesh)
            {
                draw.textureId = mesh->GetAlbedoTextureId();
            }

            instances.push_back(std::move(draw));
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

const VulkanViewport::GpuMesh* VulkanViewport::ResolveMesh(const std::string& assetId)
{
    if (assetId.empty() || !m_context || !m_context->IsInitialized())
    {
        return nullptr;
    }

    auto cached = m_meshCache.find(assetId);
    if (cached != m_meshCache.end())
    {
        return &cached->second;
    }

    if (!m_assetRegistry)
    {
        return nullptr;
    }

    const auto* meshData = m_assetRegistry->LoadMeshData(assetId);
    if (!meshData || meshData->positions.empty())
    {
        if (m_missingMeshes.emplace(assetId).second && m_context)
        {
            m_context->Log(LogSeverity::Warning,
                           "VulkanViewport: mesh data missing or unsupported for asset '" + assetId + "'");
        }
        return nullptr;
    }
    m_missingMeshes.erase(assetId);

    // glTF indices are optional. If the mesh is non-indexed, generate sequential indices.
    std::vector<uint32_t> generatedIndices;
    const std::vector<uint32_t>* indexSource = &meshData->indices;
    if (indexSource->empty())
    {
        generatedIndices.resize(meshData->positions.size());
        for (size_t i = 0; i < generatedIndices.size(); ++i)
        {
            generatedIndices[i] = static_cast<uint32_t>(i);
        }
        indexSource = &generatedIndices;
    }

    std::vector<Vertex> vertices;
    vertices.reserve(meshData->positions.size());
    for (size_t i = 0; i < meshData->positions.size(); ++i)
    {
        const auto& pos = meshData->positions[i];
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
        if (i < meshData->colors.size())
        {
            r = meshData->colors[i][0];
            g = meshData->colors[i][1];
            b = meshData->colors[i][2];
            a = meshData->colors[i][3];
        }

        float nx = 0.0f, ny = 0.0f, nz = 1.0f;
        if (i < meshData->normals.size())
        {
            nx = meshData->normals[i][0];
            ny = meshData->normals[i][1];
            nz = meshData->normals[i][2];
        }

        float u = 0.0f, v = 0.0f;
        if (i < meshData->uvs.size())
        {
            u = meshData->uvs[i][0];
            v = meshData->uvs[i][1];
        }

        vertices.push_back(Vertex{{pos[0], pos[1], pos[2]},
                                  {nx, ny, nz},
                                  {r, g, b, a},
                                  {u, v}});
    }

    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    GpuMesh mesh{};
    VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingVertexMemory = VK_NULL_HANDLE;
    VkBuffer stagingIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingIndexMemory = VK_NULL_HANDLE;
    try
    {
        const VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
        const VkDeviceSize indexSize =
            sizeof(std::uint32_t) * static_cast<VkDeviceSize>(indexSource->size());

        CreateBuffer(gpu,
                     device,
                     vertexSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer,
                     stagingVertexMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexMemory, 0, vertexSize, 0, &vData);
        std::memcpy(vData, vertices.data(), static_cast<size_t>(vertexSize));
        vkUnmapMemory(device, stagingVertexMemory);

        CreateBuffer(gpu,
                     device,
                     vertexSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     mesh.vertexBuffer,
                     mesh.vertexMemory);
        CopyBuffer(stagingVertexBuffer, mesh.vertexBuffer, vertexSize);

        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexMemory, nullptr);
        stagingVertexBuffer = VK_NULL_HANDLE;
        stagingVertexMemory = VK_NULL_HANDLE;

        CreateBuffer(gpu,
                     device,
                     indexSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer,
                     stagingIndexMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexMemory, 0, indexSize, 0, &iData);
        std::memcpy(iData, indexSource->data(), static_cast<size_t>(indexSize));
        vkUnmapMemory(device, stagingIndexMemory);

        CreateBuffer(gpu,
                     device,
                     indexSize,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     mesh.indexBuffer,
                     mesh.indexMemory);
        CopyBuffer(stagingIndexBuffer, mesh.indexBuffer, indexSize);

        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexMemory, nullptr);
        stagingIndexBuffer = VK_NULL_HANDLE;
        stagingIndexMemory = VK_NULL_HANDLE;

        mesh.indexCount = static_cast<uint32_t>(indexSource->size());      
    }
    catch (const std::exception& ex)
    {
        if (stagingVertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        }
        if (stagingVertexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, stagingVertexMemory, nullptr);
        }
        if (stagingIndexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        }
        if (stagingIndexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, stagingIndexMemory, nullptr);
        }
        if (mesh.vertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
        }
        if (mesh.vertexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, mesh.vertexMemory, nullptr);
        }
        if (mesh.indexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
        }
        if (mesh.indexMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, mesh.indexMemory, nullptr);
        }

        if (m_context)
        {
            m_context->Log(LogSeverity::Error, std::string("Mesh upload failed: ") + ex.what());
        }
        return nullptr;
    }

    auto [it, inserted] = m_meshCache.emplace(assetId, std::move(mesh));
    if (!inserted)
    {
        it->second = std::move(mesh);
    }
    return &it->second;
}

void VulkanViewport::TransitionImageLayout(VkImage image,
                                           VkFormat,
                                           VkImageLayout oldLayout,
                                           VkImageLayout newLayout)
{
    if (m_commandPool == VK_NULL_HANDLE)
    {
        throw std::runtime_error("VulkanViewport: command pool missing for texture upload");
    }

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, &cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate texture command buffer");
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        vkEndCommandBuffer(cmd);
        vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);
        throw std::runtime_error("Unsupported texture layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context->GetGraphicsQueue());

    vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);       
}

void VulkanViewport::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    if (m_commandPool == VK_NULL_HANDLE)
    {
        throw std::runtime_error("VulkanViewport: command pool missing for buffer copy");
    }

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, &cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate buffer copy command buffer");
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context->GetGraphicsQueue());

    vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);
}

void VulkanViewport::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    if (m_commandPool == VK_NULL_HANDLE)
    {
        throw std::runtime_error("VulkanViewport: command pool missing for texture copy");
    }

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, &cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate texture copy command buffer");
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context->GetGraphicsQueue());

    vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);
}

VulkanViewport::GpuTexture VulkanViewport::CreateTextureFromPixels(const unsigned char* pixels,
                                                                   uint32_t width,
                                                                   uint32_t height)
{
    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    CreateBuffer(gpu,
                 device,
                 imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingMemory);

    void* data = nullptr;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    GpuTexture texture{};
    texture.width = width;
    texture.height = height;

    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    CreateImage(gpu,
                device,
                width,
                height,
                format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                texture.image,
                texture.memory);

    TransitionImageLayout(texture.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, texture.image, width, height);
    TransitionImageLayout(texture.image,
                          format,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    texture.view = CreateImageView(device, texture.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    texture.sampler = m_textureSampler;

    if (m_textureDescriptorPool == VK_NULL_HANDLE || m_textureDescriptorSetLayout == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Texture descriptor pool/layout not available");
    }

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_textureDescriptorPool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &m_textureDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &alloc, &texture.descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate texture descriptor set");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = texture.sampler;
    imageInfo.imageView = texture.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = texture.descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    return texture;
}

const VulkanViewport::GpuTexture* VulkanViewport::ResolveTexture(const std::string& assetId)
{
    if (!m_context || !m_context->IsInitialized())
    {
        return nullptr;
    }

    if (assetId.empty())
    {
        return &m_defaultTexture;
    }

    auto cached = m_textureCache.find(assetId);
    if (cached != m_textureCache.end())
    {
        return &cached->second;
    }

    if (m_textureCache.size() + 1 >= kMaxTextureDescriptors)
    {
        if (m_missingTextures.emplace(assetId).second && m_context)
        {
            m_context->Log(LogSeverity::Warning,
                           "VulkanViewport: texture descriptor pool exhausted; using default for '" + assetId + "'");
        }
        return &m_defaultTexture;
    }

    if (!m_assetRegistry)
    {
        return &m_defaultTexture;
    }

    std::filesystem::path sourcePath;
    if (const auto* entry = m_assetRegistry->FindEntry(assetId))
    {
        sourcePath = entry->path;
    }
    else
    {
        sourcePath = std::filesystem::path(assetId);
        if (!sourcePath.is_absolute())
        {
            const auto root = m_assetRegistry->GetRootPath();
            if (!root.empty())
            {
                sourcePath = root / sourcePath;
            }
        }
    }

    std::error_code ec;
    if (sourcePath.empty() || !std::filesystem::exists(sourcePath, ec))
    {
        if (m_missingTextures.emplace(assetId).second && m_context)
        {
            m_context->Log(LogSeverity::Warning,
                           "VulkanViewport: texture asset not found '" + assetId + "'");
        }
        return &m_defaultTexture;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(sourcePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels || width <= 0 || height <= 0)
    {
        if (m_missingTextures.emplace(assetId).second && m_context)
        {
            m_context->Log(LogSeverity::Warning,
                           "VulkanViewport: failed to load texture '" + assetId + "'");
        }
        if (pixels)
        {
            stbi_image_free(pixels);
        }
        return &m_defaultTexture;
    }

    GpuTexture texture{};
    try
    {
        texture = CreateTextureFromPixels(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
    catch (const std::exception& ex)
    {
        if (m_context)
        {
            m_context->Log(LogSeverity::Error, std::string("Texture upload failed: ") + ex.what());
        }
        stbi_image_free(pixels);
        return &m_defaultTexture;
    }
    stbi_image_free(pixels);

    auto [it, inserted] = m_textureCache.emplace(assetId, std::move(texture));
    if (!inserted)
    {
        it->second = std::move(texture);
    }
    return &it->second;
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

