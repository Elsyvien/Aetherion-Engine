#include "Aetherion/Rendering/VulkanViewport.h"

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Core/Math.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/TransformComponent.h"


#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>


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

namespace Aetherion::Rendering {
namespace {
struct Vertex {
  float pos[3];
  float normal[3];
  float color[4];
  float uv[2];
};

// `VulkanViewport::kMaxLights` is private; mirror its value here locally.
constexpr uint32_t kMaxLights = 8u;

struct alignas(16) LightUniform {
  float position[4];
  float direction[4];
  float color[4];
  float spot[4];
};

struct alignas(16) FrameUniformObject {
  // Column-major view-projection matrix for GLSL.
  float viewProj[16];
  float lightDir[4];
  float lightColor[4];
  float ambientColor[4];
  float cameraPos[4];
  float frameParams[4];
  float materialParams[4];
  float lightCounts[4];
  LightUniform lights[kMaxLights];
};

constexpr uint32_t kInstanceFlagUnlit = 1u;
constexpr std::array<const char *, VulkanViewport::kPassCount> kPassNames = {
    "Opaque",
    "Picking",
    "PostProcess",
    "Overlay",
};
constexpr const char *kIconMeshId = "__editor_icon_quad";

uint32_t DecodeEntityIdFromRgba(const uint8_t *rgba) {
  return static_cast<uint32_t>(rgba[0]) |
         (static_cast<uint32_t>(rgba[1]) << 8) |
         (static_cast<uint32_t>(rgba[2]) << 16) |
         (static_cast<uint32_t>(rgba[3]) << 24);
}

void Mat4Identity(float out[16]) { Core::Math::Mat4Identity(out); }

void Mat4Mul(float out[16], const float a[16], const float b[16]) {
  Core::Math::Mat4Mul(out, a, b);
}

void Mat4Translation(float out[16], float x, float y, float z) {
  Core::Math::Mat4Translation(out, x, y, z);
}

void Mat4Scale(float out[16], float x, float y, float z) {
  Core::Math::Mat4Scale(out, x, y, z);
}

void Mat4RotationX(float out[16], float radians) {
  Core::Math::Mat4RotationX(out, radians);
}

void Mat4RotationY(float out[16], float radians) {
  Core::Math::Mat4RotationY(out, radians);
}

void Mat4RotationZ(float out[16], float radians) {
  Core::Math::Mat4RotationZ(out, radians);
}

std::array<float, 3> Mat4TransformPoint(const float m[16],
                                        const std::array<float, 3> &p) {
  return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
          m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
          m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}

void Mat4Ortho(float out[16], float left, float right, float bottom, float top,
               float zNear, float zFar) {
  Core::Math::Mat4Identity(out);
  out[0] = 2.0f / (right - left);
  // Vulkan clip space uses 0..1 depth and a flipped Y compared to OpenGL.
  out[5] = -2.0f / (top - bottom);
  out[10] = 1.0f / (zNear - zFar);
  out[12] = -(right + left) / (right - left);
  out[13] = (top + bottom) / (top - bottom);
  out[14] = zNear / (zNear - zFar);
}

void Vec3Normalize(float v[3]) { Core::Math::Vec3Normalize(v); }

void Vec3Cross(float out[3], const float a[3], const float b[3]) {
  Core::Math::Vec3Cross(out, a, b);
}

float Vec3Dot(const float a[3], const float b[3]) {
  return Core::Math::Vec3Dot(a, b);
}

void Mat4LookAt(float out[16], const float eye[3], const float center[3],
                const float up[3]) {
  float f[3] = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
  Core::Math::Vec3Normalize(f);

  float s[3];
  Core::Math::Vec3Cross(s, f, up);
  Core::Math::Vec3Normalize(s);

  float u[3];
  Core::Math::Vec3Cross(u, s, f);

  Core::Math::Mat4Identity(out);
  out[0] = s[0];
  out[4] = s[1];
  out[8] = s[2];
  out[1] = u[0];
  out[5] = u[1];
  out[9] = u[2];
  out[2] = -f[0];
  out[6] = -f[1];
  out[10] = -f[2];
  out[12] = -Core::Math::Vec3Dot(s, eye);
  out[13] = -Core::Math::Vec3Dot(u, eye);
  out[14] = Core::Math::Vec3Dot(f, eye);
}

void Mat4Perspective(float out[16], float fovRadians, float aspect, float zNear,
                     float zFar) {
  Core::Math::Mat4Identity(out);
  const float f = 1.0f / std::tan(fovRadians * 0.5f);
  out[0] = f / aspect;
  out[5] = -f;
  out[10] = zFar / (zNear - zFar);
  out[11] = -1.0f;
  out[14] = (zFar * zNear) / (zNear - zFar);
  out[15] = 0.0f;
}

uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProps{};
  vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);

  for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
    if ((typeFilter & (1u << i)) &&
        ((memProps.memoryTypes[i].propertyFlags & properties) == properties)) {
      return i;
    }
  }
  throw std::runtime_error("Failed to find suitable memory type");
}

bool HasStencilComponent(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

bool IsSrgbFormat(VkFormat format) {
  return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

bool FormatSupports(VkPhysicalDevice gpu, VkFormat format,
                    VkFormatFeatureFlags features) {
  VkFormatProperties props{};
  vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
  return (props.optimalTilingFeatures & features) == features;
}

VkFormat FindDepthFormat(VkPhysicalDevice gpu) {
  const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT,
                                 VK_FORMAT_D32_SFLOAT_S8_UINT,
                                 VK_FORMAT_D24_UNORM_S8_UINT};
  for (VkFormat format : candidates) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
    if (props.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      return format;
    }
  }
  throw std::runtime_error("Failed to find suitable depth format");
}

VkFormat FindSceneColorFormat(VkPhysicalDevice gpu) {
  const VkFormatFeatureFlags needed =
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  const VkFormat candidates[] = {
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8A8_UNORM,
  };
  for (VkFormat format : candidates) {
    if (FormatSupports(gpu, format, needed)) {
      return format;
    }
  }
  return VK_FORMAT_R8G8B8A8_UNORM;
}

struct PickingFormatInfo {
  VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
  bool isUint{false};
};

PickingFormatInfo FindPickingFormat(VkPhysicalDevice gpu) {
  const VkFormatFeatureFlags baseNeeded =
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
      VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  if (FormatSupports(gpu, VK_FORMAT_R32_UINT, baseNeeded)) {
    return {VK_FORMAT_R32_UINT, true};
  }
  const VkFormatFeatureFlags rgbaNeeded =
      baseNeeded | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  if (FormatSupports(gpu, VK_FORMAT_R8G8B8A8_UNORM, rgbaNeeded)) {
    return {VK_FORMAT_R8G8B8A8_UNORM, false};
  }
  return {VK_FORMAT_R8G8B8A8_UNORM, false};
}

void CreateImage(VkPhysicalDevice gpu, VkDevice device, uint32_t width,
                 uint32_t height, VkFormat format, VkImageTiling tiling,
                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                 VkImage &outImage, VkDeviceMemory &outMemory) {
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

  if (vkCreateImage(device, &imageInfo, nullptr, &outImage) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create image");
  }

  VkMemoryRequirements memReq{};
  vkGetImageMemoryRequirements(device, outImage, &memReq);

  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = memReq.size;
  alloc.memoryTypeIndex =
      FindMemoryType(gpu, memReq.memoryTypeBits, properties);

  if (vkAllocateMemory(device, &alloc, nullptr, &outMemory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate image memory");
  }

  vkBindImageMemory(device, outImage, outMemory, 0);
}

VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format,
                            VkImageAspectFlags aspect) {
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
  if (vkCreateImageView(device, &view, nullptr, &imageView) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create image view");
  }
  return imageView;
}

void CreateBuffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer &outBuffer, VkDeviceMemory &outMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer");
  }

  VkMemoryRequirements memReq{};
  vkGetBufferMemoryRequirements(device, outBuffer, &memReq);

  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = memReq.size;
  alloc.memoryTypeIndex =
      FindMemoryType(gpu, memReq.memoryTypeBits, properties);

  if (vkAllocateMemory(device, &alloc, nullptr, &outMemory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate buffer memory");
  }

  vkBindBufferMemory(device, outBuffer, outMemory, 0);
}

VkSurfaceFormatKHR
ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) {
  for (const auto &f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }
  for (const auto &f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }
  return formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM,
                                              VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                         : formats[0];
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> &modes) {
  for (auto m : modes) {
    if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
      return m;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR &caps, int width,
                        int height) {
  if (caps.currentExtent.width != UINT32_MAX) {
    return caps.currentExtent;
  }

  VkExtent2D extent{};
  extent.width = static_cast<uint32_t>(width);
  extent.height = static_cast<uint32_t>(height);

  extent.width = std::max(caps.minImageExtent.width,
                          std::min(caps.maxImageExtent.width, extent.width));
  extent.height = std::max(caps.minImageExtent.height,
                           std::min(caps.maxImageExtent.height, extent.height));

  return extent;
}
} // namespace

VulkanViewport::VulkanViewport(
    std::shared_ptr<VulkanContext> context,
    std::shared_ptr<Assets::AssetRegistry> assetRegistry)
    : m_context(std::move(context)), m_assetRegistry(std::move(assetRegistry)) {
  if (m_context) {
    m_verboseLogging = m_context->IsLoggingEnabled();
  }
}

VulkanViewport::~VulkanViewport() { Shutdown(); }

void VulkanViewport::Initialize(void *nativeHandle, int width, int height) {
  if (!m_context || !m_context->IsInitialized()) {
    throw std::runtime_error("VulkanViewport: VulkanContext not initialized");
  }

  try {
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
  } catch (const std::exception &ex) {
    m_context->Log(LogSeverity::Error,
                   std::string("VulkanViewport: initialization failed - ") +
                       ex.what());
    Shutdown();
    throw;
  }
}

void VulkanViewport::Shutdown() {
  if (m_shutdown) {
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

void VulkanViewport::Resize(int width, int height) {
  m_surfaceWidth = width;
  m_surfaceHeight = height;

  if (!m_context || !m_context->IsInitialized() ||
      m_surface == VK_NULL_HANDLE) {
    return;
  }

#ifdef __APPLE__
  UpdateMetalLayerSize(width, height);
#endif

  if (width <= 0 || height <= 0) {
    m_context->Log(LogSeverity::Warning,
                   "VulkanViewport: resize ignored (surface has zero area)");
    return;
  }

  if (m_verboseLogging) {
    m_context->Log(LogSeverity::Info,
                   "VulkanViewport: recreating renderer for " +
                       std::to_string(width) + "x" + std::to_string(height));
  }

  try {
    RecreateRenderer(width, height);
    m_ready = true;
    m_needsSwapchainRecreate = false; // Clear flag since we just recreated.
  } catch (const std::exception &ex) {
    m_context->Log(
        LogSeverity::Error,
        std::string("VulkanViewport: swapchain recreation failed - ") +
            ex.what());
    m_ready = false;
  }
  m_frameIndex = 0;
  m_waitingForValidExtent = false;
}

void VulkanViewport::RenderFrame(float deltaTimeSeconds,
                                 const RenderView &view) {
  if (!m_ready || m_swapchain == VK_NULL_HANDLE) {
    return;
  }

  // If swapchain was marked out-of-date, recreate it.
  // DestroyDeviceResources will call vkDeviceWaitIdle internally.
  if (m_needsSwapchainRecreate) {
    m_needsSwapchainRecreate = false;
    TryRecoverSwapchain();
    return;
  }

  static bool s_loggedFirstFrame = false;

  if (m_swapchainExtent.width == 0 || m_swapchainExtent.height == 0) {
    if (!m_waitingForValidExtent) {
      if (m_verboseLogging) {
        m_context->Log(LogSeverity::Warning,
                       "VulkanViewport: swapchain extent is zero; skipping "
                       "frame until resize");
      }
      m_waitingForValidExtent = true;
    }
    return;
  }
  m_waitingForValidExtent = false;

  if (!s_loggedFirstFrame && m_context) {
    m_context->Log(LogSeverity::Info, "VulkanViewport: entering render loop");
    s_loggedFirstFrame = true;
  }

  m_timeSeconds += deltaTimeSeconds;
  const auto instances = InstancesFromView(view, m_timeSeconds);
  UpdateSelectionBuffer(instances, view);
  UpdateLightGizmoBuffer(view);

  VkDevice device = m_context->GetDevice();
  VkQueue graphicsQueue = m_context->GetGraphicsQueue();
  VkQueue presentQueue = m_context->GetPresentQueue();

  VkFence inFlight = m_inFlight[m_frameIndex];
  // Wait for previous frame using this slot to complete.
  // Use a short timeout to keep the UI responsive; if not ready, skip this
  // frame.
  VkResult fenceWait =
      vkWaitForFences(device, 1, &inFlight, VK_TRUE, 1'000'000ULL); // 1ms
  if (fenceWait == VK_TIMEOUT) {
    // Previous frame not done yet, skip to keep UI responsive.
    return;
  }
  if (fenceWait != VK_SUCCESS) {
    throw std::runtime_error("vkWaitForFences failed");
  }

  ProcessDeferredDeletions();

  if (m_timestampsSupported && m_queryPools[m_frameIndex] != VK_NULL_HANDLE &&
      m_frameStats[m_frameIndex].valid) {
    std::array<uint64_t, kPassCount * 2> results{};
    const VkResult queryRes = vkGetQueryPoolResults(
        device, m_queryPools[m_frameIndex], 0, kPassCount * 2, sizeof(results),
        results.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    FrameStats stats = m_frameStats[m_frameIndex];
    if (queryRes == VK_SUCCESS) {
      double gpuTotalMs = 0.0;
      for (uint32_t i = 0; i < kPassCount; ++i) {
        const uint64_t start = results[i * 2 + 0];
        const uint64_t end = results[i * 2 + 1];
        const double gpuMs = (end > start) ? (static_cast<double>(end - start) *
                                              m_timestampPeriod / 1'000'000.0)
                                           : 0.0;
        stats.passes[i].gpuMs = gpuMs;
        gpuTotalMs += gpuMs;
      }
      stats.gpuTotalMs = gpuTotalMs;
      stats.valid = true;
    } else {
      stats.valid = false;
      stats.gpuTotalMs = 0.0;
      for (auto &pass : stats.passes) {
        pass.gpuMs = 0.0;
      }
    }
    m_lastFrameStats = stats;
  }

  if (m_pickReadbacks[m_frameIndex].inFlight &&
      m_pickingReadbackMemories[m_frameIndex] != VK_NULL_HANDLE) {
    void *mapped = nullptr;
    if (vkMapMemory(device, m_pickingReadbackMemories[m_frameIndex], 0,
                    VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS &&
        mapped) {
      Core::EntityId id = 0;
      if (m_pickingFormatIsUint) {
        uint32_t raw = 0;
        std::memcpy(&raw, mapped, sizeof(uint32_t));
        id = static_cast<Core::EntityId>(raw);
      } else {
        auto *bytes = static_cast<const uint8_t *>(mapped);
        id = static_cast<Core::EntityId>(DecodeEntityIdFromRgba(bytes));
      }
      vkUnmapMemory(device, m_pickingReadbackMemories[m_frameIndex]);
      m_lastPickResult.entityId = id;
      m_lastPickResult.x = m_pickReadbacks[m_frameIndex].x;
      m_lastPickResult.y = m_pickReadbacks[m_frameIndex].y;
      m_lastPickResult.valid = true;
    }
    m_pickReadbacks[m_frameIndex].inFlight = false;
  }

  uint32_t imageIndex = 0;
  VkResult acquire = vkAcquireNextImageKHR(device, m_swapchain, 1'000'000ULL,
                                           m_imageAvailable[m_frameIndex],
                                           VK_NULL_HANDLE, &imageIndex); // 1ms
  if (acquire == VK_TIMEOUT || acquire == VK_NOT_READY) {
    // Image not available, skip frame.
    return;
  }
  if (acquire == VK_ERROR_OUT_OF_DATE_KHR ||
      acquire == VK_ERROR_SURFACE_LOST_KHR) {
    // Swapchain is out of date; mark for recreation and skip this frame.
    // The next Resize() call or next RenderFrame will handle it.
    m_needsSwapchainRecreate = true;
    return;
  }
  if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("vkAcquireNextImageKHR failed");
  }
  if (acquire == VK_SUBOPTIMAL_KHR) {
    // Continue rendering this frame, but recreate the swapchain next frame.
    m_needsSwapchainRecreate = true;
  }

  if (imageIndex >= m_imagesInFlight.size() ||
      imageIndex >= m_renderFinishedPerImage.size()) {
    m_context->Log(
        LogSeverity::Error,
        "VulkanViewport: acquired image index out of range for sync objects");
    return;
  }

  // Ensure the acquired swapchain image is no longer in use by a previous
  // frame.
  if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    const VkResult imgWait = vkWaitForFences(
        device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    if (imgWait != VK_SUCCESS) {
      throw std::runtime_error("vkWaitForFences failed for swapchain image");
    }
  }

  // Now that we know a frame will be submitted, reset the fence for this frame.
  vkResetFences(device, 1, &inFlight);
  m_imagesInFlight[imageIndex] = inFlight;

  UpdateUniformBuffer(m_frameIndex, view);

  vkResetCommandBuffer(m_commandBuffers[m_frameIndex], 0);
  m_frameStats[m_frameIndex] = {};
  for (uint32_t i = 0; i < kPassCount; ++i) {
    m_frameStats[m_frameIndex].passes[i].name = kPassNames[i];
  }
  const auto cpuStart = std::chrono::steady_clock::now();
  RecordCommandBuffer(imageIndex, instances);
  const auto cpuEnd = std::chrono::steady_clock::now();
  m_frameStats[m_frameIndex].cpuTotalMs =
      std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();

  VkSemaphore waitSem = m_imageAvailable[m_frameIndex];
  VkSemaphore signalSem = m_renderFinishedPerImage[imageIndex];

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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
  if (submitRes == VK_ERROR_DEVICE_LOST) {
    m_context->Log(
        LogSeverity::Error,
        "VulkanViewport: device lost during submit; attempting to recover");
    m_ready = false;
    TryRecoverSwapchain();
    return;
  }

  if (submitRes != VK_SUCCESS) {
    throw std::runtime_error("vkQueueSubmit failed");
  }
  m_frameStats[m_frameIndex].valid = true;

  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &signalSem;
  present.swapchainCount = 1;
  present.pSwapchains = &m_swapchain;
  present.pImageIndices = &imageIndex;

  VkResult pres = vkQueuePresentKHR(presentQueue, &present);
  if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
    // Swapchain is out of date or suboptimal (e.g., window resized).
    // Mark for recreation and continue; next frame will handle it.
    m_needsSwapchainRecreate = true;
    // For SUBOPTIMAL we still presented successfully, so advance the frame
    // index.
    if (pres == VK_SUBOPTIMAL_KHR) {
      m_frameIndex = (m_frameIndex + 1) % kMaxFramesInFlight;
    }
    return;
  } else if (pres == VK_ERROR_SURFACE_LOST_KHR ||
             pres == VK_ERROR_DEVICE_LOST) {
    m_context->Log(LogSeverity::Error, "VulkanViewport: surface or device lost "
                                       "during present; attempting to recover");
    TryRecoverSwapchain();
    return;
  } else if (pres != VK_SUCCESS) {
    m_context->Log(LogSeverity::Error,
                   "VulkanViewport: vkQueuePresentKHR returned " +
                       std::to_string(static_cast<int>(pres)));
    // Non-fatal; mark for recreation and try again next frame.
    m_needsSwapchainRecreate = true;
    return;
  }

  m_frameIndex = (m_frameIndex + 1) % kMaxFramesInFlight;
}

void VulkanViewport::RequestPick(uint32_t x, uint32_t y) noexcept {
  m_pendingPick.pending = true;
  m_pendingPick.x = x;
  m_pendingPick.y = y;
}

void VulkanViewport::RecreateRenderer(int width, int height) {
  // Wait for any in-flight work to complete before destroying resources.
  if (m_context && m_context->IsInitialized()) {
    vkDeviceWaitIdle(m_context->GetDevice());
  }

  DestroyDeviceResources();

  try {
    m_context->EnsureSurfaceCompatibility(m_surface);

#ifdef __APPLE__
    UpdateMetalLayerSize(width, height);
#endif

    CreateSwapchain(width, height);
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateCommandPoolAndBuffers();
    CreateMeshBuffers();
    CreateLineBuffers();
    CreateUniformBuffers();
    CreateDescriptorPoolAndSets();
    CreateTextureDescriptorPool();
    CreateTextureResources();
    CreateSceneResources();
    CreatePickingResources();
    CreatePipeline();
    CreateFramebuffers();
    UpdatePostProcessDescriptorSets();
    CreateSyncObjects();
    CreateQueryPools();

    m_ready = true;
    m_frameIndex = 0;
    m_waitingForValidExtent = false;
  } catch (...) {
    DestroyDeviceResources();
    throw;
  }
}

bool VulkanViewport::TryRecoverSwapchain() {
  if (m_surface == VK_NULL_HANDLE || !m_context ||
      !m_context->IsInitialized()) {
    m_ready = false;
    return false;
  }

  if (m_surfaceWidth <= 0 || m_surfaceHeight <= 0) {
    m_ready = false;
    return false;
  }

  try {
    RecreateRenderer(m_surfaceWidth, m_surfaceHeight);
    return true;
  } catch (const std::exception &ex) {
    m_context->Log(
        LogSeverity::Error,
        std::string("VulkanViewport: failed to recover swapchain - ") +
            ex.what());
    m_ready = false;
    return false;
  }
}

void VulkanViewport::DestroyDeviceResources() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
  }

  FlushDeferredDeletions();

  DestroySwapchainResources();
  DestroyMeshCache();
  DestroyTextureCache();

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (device != VK_NULL_HANDLE && m_uniformMemories[i] != VK_NULL_HANDLE &&
        m_uniformMapped[i] != nullptr) {
      vkUnmapMemory(device, m_uniformMemories[i]);
    }
    m_uniformMapped[i] = nullptr;

    if (device != VK_NULL_HANDLE && m_uniformBuffers[i] != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
    }
    m_uniformBuffers[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_uniformMemories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, m_uniformMemories[i], nullptr);
    }
    m_uniformMemories[i] = VK_NULL_HANDLE;
  }

  if (device != VK_NULL_HANDLE && m_indexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_indexBuffer, nullptr);
  }
  m_indexBuffer = VK_NULL_HANDLE;
  if (device != VK_NULL_HANDLE && m_indexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_indexMemory, nullptr);
  }
  m_indexMemory = VK_NULL_HANDLE;
  m_defaultIndexCount = 0;

  if (device != VK_NULL_HANDLE && m_vertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_vertexBuffer, nullptr);
  }
  m_vertexBuffer = VK_NULL_HANDLE;
  if (device != VK_NULL_HANDLE && m_vertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_vertexMemory, nullptr);
  }
  m_vertexMemory = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_iconMesh.vertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_iconMesh.vertexBuffer, nullptr);
  }
  if (device != VK_NULL_HANDLE && m_iconMesh.vertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_iconMesh.vertexMemory, nullptr);
  }
  if (device != VK_NULL_HANDLE && m_iconMesh.indexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_iconMesh.indexBuffer, nullptr);
  }
  if (device != VK_NULL_HANDLE && m_iconMesh.indexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_iconMesh.indexMemory, nullptr);
  }
  m_iconMesh = {};

  if (device != VK_NULL_HANDLE && m_lineVertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_lineVertexBuffer, nullptr);
  }
  m_lineVertexBuffer = VK_NULL_HANDLE;
  if (device != VK_NULL_HANDLE && m_lineVertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_lineVertexMemory, nullptr);
  }
  m_lineVertexMemory = VK_NULL_HANDLE;
  m_lineVertexCount = 0;

  if (device != VK_NULL_HANDLE && m_selectionVertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_selectionVertexBuffer, nullptr);
  }
  m_selectionVertexBuffer = VK_NULL_HANDLE;
  if (device != VK_NULL_HANDLE && m_selectionVertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_selectionVertexMemory, nullptr);
  }
  m_selectionVertexMemory = VK_NULL_HANDLE;
  m_selectionVertexCount = 0;

  if (device != VK_NULL_HANDLE && m_lightGizmoVertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, m_lightGizmoVertexBuffer, nullptr);
  }
  m_lightGizmoVertexBuffer = VK_NULL_HANDLE;
  if (device != VK_NULL_HANDLE && m_lightGizmoVertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, m_lightGizmoVertexMemory, nullptr);
  }
  m_lightGizmoVertexMemory = VK_NULL_HANDLE;
  m_lightGizmoVertexCount = 0;

  for (auto &pool : m_descriptorPools) {
    if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, pool, nullptr);
    }
    pool = VK_NULL_HANDLE;
  }
  m_descriptorSets.fill(VK_NULL_HANDLE);
  m_postProcessDescriptorSets.fill(VK_NULL_HANDLE);

  for (auto pool : m_textureDescriptorPools) {
    if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, pool, nullptr);
    }
  }
  m_textureDescriptorPools.clear();
  m_activeTextureDescriptorPool = 0;

  if (device != VK_NULL_HANDLE && m_descriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
  }
  m_descriptorSetLayout = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE &&
      m_textureDescriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_textureDescriptorSetLayout, nullptr);
  }
  m_textureDescriptorSetLayout = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE &&
      m_postProcessDescriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_postProcessDescriptorSetLayout,
                                 nullptr);
  }
  m_postProcessDescriptorSetLayout = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_textureSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_textureSampler, nullptr);
  }
  m_textureSampler = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_postProcessSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_postProcessSampler, nullptr);
  }
  m_postProcessSampler = VK_NULL_HANDLE;

  for (auto &pool : m_queryPools) {
    if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
      vkDestroyQueryPool(device, pool, nullptr);
    }
    pool = VK_NULL_HANDLE;
  }
  m_timestampsSupported = false;
  m_timestampPeriod = 0.0f;

  for (auto fence : m_inFlight) {
    if (device != VK_NULL_HANDLE && fence != VK_NULL_HANDLE) {
      vkDestroyFence(device, fence, nullptr);
    }
  }
  m_inFlight.clear();

  for (auto sem : m_imageAvailable) {
    if (device != VK_NULL_HANDLE && sem != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, sem, nullptr);
    }
  }
  m_imageAvailable.clear();

  m_imagesInFlight.clear();

  if (device != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, m_commandPool, nullptr);
  }
  m_commandPool = VK_NULL_HANDLE;
  m_commandBuffers.clear();

  m_ready = false;
  m_waitingForValidExtent = false;
}

void VulkanViewport::DestroyMeshCache() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;
  for (auto &entry : m_meshCache) {
    auto &mesh = entry.second;
    if (device != VK_NULL_HANDLE && mesh.vertexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
    }
    if (device != VK_NULL_HANDLE && mesh.vertexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, mesh.vertexMemory, nullptr);
    }
    if (device != VK_NULL_HANDLE && mesh.indexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
    }
    if (device != VK_NULL_HANDLE && mesh.indexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, mesh.indexMemory, nullptr);
    }
    mesh = {};
  }
  m_meshCache.clear();
  m_missingMeshes.clear();
}

void VulkanViewport::DestroyTextureCache() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;

  auto destroyTexture = [device](GpuTexture &texture) {
    if (device != VK_NULL_HANDLE && texture.descriptorSet != VK_NULL_HANDLE &&
        texture.descriptorPool != VK_NULL_HANDLE) {
      vkFreeDescriptorSets(device, texture.descriptorPool, 1,
                           &texture.descriptorSet);
    }
    if (device != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, texture.view, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) {
      vkDestroyImage(device, texture.image, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, texture.memory, nullptr);
    }
    texture = {};
  };

  destroyTexture(m_defaultTexture);

  for (auto &entry : m_textureCache) {
    destroyTexture(entry.second);
  }
  m_textureCache.clear();
  m_missingTextures.clear();
}

void VulkanViewport::DestroySceneResources() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (device != VK_NULL_HANDLE && m_sceneColorViews[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, m_sceneColorViews[i], nullptr);
    }
    m_sceneColorViews[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_sceneColorImages[i] != VK_NULL_HANDLE) {
      vkDestroyImage(device, m_sceneColorImages[i], nullptr);
    }
    m_sceneColorImages[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_sceneColorMemories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, m_sceneColorMemories[i], nullptr);
    }
    m_sceneColorMemories[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_sceneDepthViews[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, m_sceneDepthViews[i], nullptr);
    }
    m_sceneDepthViews[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_sceneDepthImages[i] != VK_NULL_HANDLE) {
      vkDestroyImage(device, m_sceneDepthImages[i], nullptr);
    }
    m_sceneDepthImages[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_sceneDepthMemories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, m_sceneDepthMemories[i], nullptr);
    }
    m_sceneDepthMemories[i] = VK_NULL_HANDLE;
  }
}

void VulkanViewport::DestroyPickingResources() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (device != VK_NULL_HANDLE &&
        m_pickingReadbackBuffers[i] != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, m_pickingReadbackBuffers[i], nullptr);
    }
    m_pickingReadbackBuffers[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE &&
        m_pickingReadbackMemories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, m_pickingReadbackMemories[i], nullptr);
    }
    m_pickingReadbackMemories[i] = VK_NULL_HANDLE;
    m_pickReadbacks[i] = {};

    if (device != VK_NULL_HANDLE && m_pickingViews[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, m_pickingViews[i], nullptr);
    }
    m_pickingViews[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pickingImages[i] != VK_NULL_HANDLE) {
      vkDestroyImage(device, m_pickingImages[i], nullptr);
    }
    m_pickingImages[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pickingMemories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, m_pickingMemories[i], nullptr);
    }
    m_pickingMemories[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pickingDepthViews[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, m_pickingDepthViews[i], nullptr);
    }
    m_pickingDepthViews[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE && m_pickingDepthImages[i] != VK_NULL_HANDLE) {
      vkDestroyImage(device, m_pickingDepthImages[i], nullptr);
    }
    m_pickingDepthImages[i] = VK_NULL_HANDLE;

    if (device != VK_NULL_HANDLE &&
        m_pickingDepthMemories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, m_pickingDepthMemories[i], nullptr);
    }
    m_pickingDepthMemories[i] = VK_NULL_HANDLE;
  }

  m_lastPickResult.valid = false;
}

void VulkanViewport::ProcessDeferredDeletions() {
  if (m_deferredDeletions.empty()) {
    return;
  }

  for (auto it = m_deferredDeletions.begin();
       it != m_deferredDeletions.end();) {
    if (it->framesRemaining > 0) {
      --it->framesRemaining;
    }

    if (it->framesRemaining == 0) {
      if (it->callback) {
        it->callback();
      }
      it = m_deferredDeletions.erase(it);
    } else {
      ++it;
    }
  }
}

void VulkanViewport::EnqueueDeletion(std::function<void()> &&callback,
                                     uint32_t frames) {
  if (!callback) {
    return;
  }
  DeferredDeletion entry{};
  entry.framesRemaining = std::max<uint32_t>(frames, 1);
  entry.callback = std::move(callback);
  m_deferredDeletions.push_back(std::move(entry));
}

void VulkanViewport::FlushDeferredDeletions() {
  for (auto &entry : m_deferredDeletions) {
    if (entry.callback) {
      entry.callback();
    }
  }
  m_deferredDeletions.clear();
}

void VulkanViewport::HandleAssetChanges(
    const std::vector<Assets::AssetRegistry::AssetChange> &changes) {
  if (changes.empty()) {
    return;
  }

  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;
  if (device == VK_NULL_HANDLE) {
    return;
  }

  auto destroyMesh = [this, device](GpuMesh &mesh) {
    if (mesh.vertexBuffer == VK_NULL_HANDLE &&
        mesh.indexBuffer == VK_NULL_HANDLE) {
      mesh = {};
      return;
    }
    VkBuffer vertexBuffer = mesh.vertexBuffer;
    VkDeviceMemory vertexMemory = mesh.vertexMemory;
    VkBuffer indexBuffer = mesh.indexBuffer;
    VkDeviceMemory indexMemory = mesh.indexMemory;
    EnqueueDeletion(
        [device, vertexBuffer, vertexMemory, indexBuffer, indexMemory]() {
          if (device != VK_NULL_HANDLE && vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
          }
          if (device != VK_NULL_HANDLE && vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, vertexMemory, nullptr);
          }
          if (device != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, indexBuffer, nullptr);
          }
          if (device != VK_NULL_HANDLE && indexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, indexMemory, nullptr);
          }
        });
    mesh = {};
  };

  auto destroyTexture = [this, device](GpuTexture &texture) {
    if (texture.image == VK_NULL_HANDLE && texture.view == VK_NULL_HANDLE) {
      texture = {};
      return;
    }
    VkImage image = texture.image;
    VkDeviceMemory memory = texture.memory;
    VkImageView view = texture.view;
    VkDescriptorSet descriptorSet = texture.descriptorSet;
    VkDescriptorPool descriptorPool = texture.descriptorPool;
    EnqueueDeletion(
        [device, image, memory, view, descriptorSet, descriptorPool]() {
          if (device != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE &&
              descriptorPool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
          }
          if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
          }
          if (device != VK_NULL_HANDLE && image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
          }
          if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
          }
        });
    texture = {};
  };

  for (const auto &change : changes) {
    const bool invalidate =
        change.kind == Assets::AssetRegistry::AssetChange::Kind::Removed ||
        change.kind == Assets::AssetRegistry::AssetChange::Kind::Modified ||
        change.kind == Assets::AssetRegistry::AssetChange::Kind::Moved;
    if (!invalidate) {
      continue;
    }

    if (change.type == Assets::AssetRegistry::AssetType::Mesh) {
      if (auto it = m_meshCache.find(change.id); it != m_meshCache.end()) {
        destroyMesh(it->second);
        m_meshCache.erase(it);
      }
      m_missingMeshes.erase(change.id);
    } else if (change.type == Assets::AssetRegistry::AssetType::Texture) {
      if (auto it = m_textureCache.find(change.id);
          it != m_textureCache.end()) {
        destroyTexture(it->second);
        m_textureCache.erase(it);
      }
      m_missingTextures.erase(change.id);
    }
  }
}

void VulkanViewport::DestroySwapchainResources() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;

  for (auto fb : m_framebuffers) {
    if (device != VK_NULL_HANDLE && fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
  }
  m_framebuffers.clear();

  for (auto &fb : m_sceneFramebuffers) {
    if (device != VK_NULL_HANDLE && fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
    fb = VK_NULL_HANDLE;
  }

  for (auto &fb : m_pickingFramebuffers) {
    if (device != VK_NULL_HANDLE && fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
    fb = VK_NULL_HANDLE;
  }

  if (device != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_pipeline, nullptr);
  }
  m_pipeline = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_linePipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_linePipeline, nullptr);
  }
  m_linePipeline = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_overlayPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_overlayPipeline, nullptr);
  }
  m_overlayPipeline = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_pickingPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_pickingPipeline, nullptr);
  }
  m_pickingPipeline = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_pickingPipelineUint != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_pickingPipelineUint, nullptr);
  }
  m_pickingPipelineUint = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_postProcessPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_postProcessPipeline, nullptr);
  }
  m_postProcessPipeline = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_postProcessPipelineUint != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_postProcessPipelineUint, nullptr);
  }
  m_postProcessPipelineUint = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
  }
  m_pipelineLayout = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE &&
      m_postProcessPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_postProcessPipelineLayout, nullptr);
  }
  m_postProcessPipelineLayout = VK_NULL_HANDLE;

  DestroySceneResources();
  DestroyPickingResources();

  if (device != VK_NULL_HANDLE && m_sceneRenderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_sceneRenderPass, nullptr);
  }
  m_sceneRenderPass = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_postProcessRenderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_postProcessRenderPass, nullptr);
  }
  m_postProcessRenderPass = VK_NULL_HANDLE;

  if (device != VK_NULL_HANDLE && m_pickingRenderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_pickingRenderPass, nullptr);
  }
  m_pickingRenderPass = VK_NULL_HANDLE;

  DestroySwapchain();
}

void VulkanViewport::DestroySurface() {
  if (m_surface != VK_NULL_HANDLE && m_context &&
      m_context->GetInstance() != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
  }
  m_surface = VK_NULL_HANDLE;
}

void VulkanViewport::CreateSurface(void *nativeHandle) {
#ifdef _WIN32
  HWND hwnd = reinterpret_cast<HWND>(nativeHandle);
  if (!hwnd) {
    throw std::runtime_error("VulkanViewport: invalid HWND");
  }

  VkWin32SurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hwnd = hwnd;
  createInfo.hinstance = GetModuleHandle(nullptr);

  if (vkCreateWin32SurfaceKHR(m_context->GetInstance(), &createInfo, nullptr,
                              &m_surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Win32 Vulkan surface");
  }
#elif defined(__APPLE__)
  if (!nativeHandle) {
    throw std::runtime_error("VulkanViewport: invalid native view");
  }

  // Qt provides a Cocoa view handle (NSView*) as WId on macOS.
  // Create a CAMetalLayer for that view and build a VK_EXT_metal_surface
  // surface.
  id view = reinterpret_cast<id>(nativeHandle);
  Class cametalLayerClass =
      reinterpret_cast<Class>(objc_getClass("CAMetalLayer"));
  if (!cametalLayerClass) {
    throw std::runtime_error(
        "VulkanViewport: CAMetalLayer class not available");
  }

  const SEL selSetWantsLayer = sel_registerName("setWantsLayer:");
  const SEL selSetLayer = sel_registerName("setLayer:");
  const SEL selLayer = sel_registerName("layer");
  const SEL selIsKindOfClass = sel_registerName("isKindOfClass:");
  const SEL selAlloc = sel_registerName("alloc");
  const SEL selInit = sel_registerName("init");

  // Ensure the view is layer-backed.
  reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(
      view, selSetWantsLayer, YES);

  id existingLayer =
      reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(view, selLayer);
  bool isMetalLayer = false;
  if (existingLayer) {
    isMetalLayer = reinterpret_cast<BOOL (*)(id, SEL, Class)>(objc_msgSend)(
        existingLayer, selIsKindOfClass, cametalLayerClass);
  }

  id metalLayer = existingLayer;
  if (!isMetalLayer) {
    // Create a new CAMetalLayer instance: [[CAMetalLayer alloc] init]
    id alloced = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(
        reinterpret_cast<id>(cametalLayerClass), selAlloc);
    if (!alloced) {
      throw std::runtime_error("VulkanViewport: CAMetalLayer alloc failed");
    }
    metalLayer =
        reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(alloced, selInit);
    if (!metalLayer) {
      throw std::runtime_error("VulkanViewport: CAMetalLayer init failed");
    }
    // Attach the layer to the view.
    reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(view, selSetLayer,
                                                          metalLayer);
  }

  // Configure the Metal layer for proper display.
  const SEL selSetOpaque = sel_registerName("setOpaque:");
  const SEL selSetContentsScale = sel_registerName("setContentsScale:");
  const SEL selBackingScaleFactor = sel_registerName("backingScaleFactor");
  const SEL selWindow = sel_registerName("window");

  reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(metalLayer,
                                                          selSetOpaque, YES);

  id window = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(view, selWindow);
  if (window) {
    double scale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(
        window, selBackingScaleFactor);
    if (scale > 0.0) {
      reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(
          metalLayer, selSetContentsScale, scale);
    }
  }

  m_metalLayer = metalLayer;
  UpdateMetalLayerSize(m_surfaceWidth, m_surfaceHeight);

  VkMetalSurfaceCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
  createInfo.pLayer = reinterpret_cast<CAMetalLayer *>(metalLayer);

  VkResult metalRes = vkCreateMetalSurfaceEXT(m_context->GetInstance(),
                                              &createInfo, nullptr, &m_surface);
  if (metalRes != VK_SUCCESS) {
    // Fallback: create macOS surface with NSView (MoltenVK path).
    VkMacOSSurfaceCreateInfoMVK macInfo{};
    macInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    macInfo.pView = nativeHandle;
    if (vkCreateMacOSSurfaceMVK(m_context->GetInstance(), &macInfo, nullptr,
                                &m_surface) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create Vulkan surface on macOS "
                               "(Metal and MVK fallback failed)");
    }
  }
#else
  (void)nativeHandle;
  throw std::runtime_error(
      "VulkanViewport: platform not supported in this build");
#endif

  m_context->EnsureSurfaceCompatibility(m_surface);
}

void VulkanViewport::CreateSwapchain(int width, int height) {
  auto support = m_context->QuerySwapchainSupport(m_surface);
  if (support.formats.empty() || support.presentModes.empty()) {
    m_swapchain = VK_NULL_HANDLE;
    m_swapchainExtent = {0, 0};
    m_swapchainImages.clear();
    m_swapchainImageViews.clear();
    m_renderFinishedPerImage.clear();
    m_imagesInFlight.clear();
    throw std::runtime_error(
        "VulkanViewport: swapchain support incomplete for this surface");
  }

  const VkSurfaceCapabilitiesKHR &caps = support.capabilities;
  VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.formats);
  VkPresentModeKHR presentMode = ChoosePresentMode(support.presentModes);
  VkExtent2D extent = ChooseExtent(caps, width, height);

  if (extent.width == 0 || extent.height == 0) {
    m_swapchain = VK_NULL_HANDLE;
    m_swapchainExtent = {0, 0};
    m_swapchainImages.clear();
    m_swapchainImageViews.clear();
    m_renderFinishedPerImage.clear();
    m_imagesInFlight.clear();
    throw std::runtime_error("VulkanViewport: swapchain extent is zero; "
                             "surface too small/minimized");
  }

  if (m_verboseLogging) {
    m_context->Log(
        LogSeverity::Info,
        "VulkanViewport: creating swapchain " + std::to_string(extent.width) +
            "x" + std::to_string(extent.height) + " (" +
            std::to_string(support.formats.size()) + " formats, " +
            std::to_string(support.presentModes.size()) +
            " present modes, min images " + std::to_string(caps.minImageCount) +
            ", max images " +
            (caps.maxImageCount == 0 ? std::string("unbounded")
                                     : std::to_string(caps.maxImageCount)) +
            ")");
  }

  uint32_t imageCount = std::max<uint32_t>(caps.minImageCount, 2);
  if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
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
  if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
    create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create.queueFamilyIndexCount = 2;
    create.pQueueFamilyIndices = queueFamilyIndices.data();
  } else {
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  VkSurfaceTransformFlagBitsKHR preTransform =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  if ((caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0) {
    preTransform =
        static_cast<VkSurfaceTransformFlagBitsKHR>(caps.currentTransform);
  }
  if (preTransform == 0) {
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }
  create.preTransform = preTransform;
  create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create.presentMode = presentMode;
  create.clipped = VK_TRUE;

  if (vkCreateSwapchainKHR(m_context->GetDevice(), &create, nullptr,
                           &m_swapchain) != VK_SUCCESS) {
    m_swapchain = VK_NULL_HANDLE;
    m_swapchainExtent = {0, 0};
    m_swapchainImages.clear();
    m_swapchainImageViews.clear();
    m_renderFinishedPerImage.clear();
    m_imagesInFlight.clear();
    throw std::runtime_error(
        "VulkanViewport: failed to create swapchain for current surface");
  }

  uint32_t actualCount = 0;
  vkGetSwapchainImagesKHR(m_context->GetDevice(), m_swapchain, &actualCount,
                          nullptr);
  m_swapchainImages.resize(actualCount);
  vkGetSwapchainImagesKHR(m_context->GetDevice(), m_swapchain, &actualCount,
                          m_swapchainImages.data());

  m_swapchainFormat = surfaceFormat.format;
  m_swapchainExtent = extent;
  m_sceneColorFormat = FindSceneColorFormat(m_context->GetPhysicalDevice());
  const auto pickInfo = FindPickingFormat(m_context->GetPhysicalDevice());
  m_pickingFormat = pickInfo.format;
  m_pickingFormatIsUint = pickInfo.isUint;

  m_swapchainImageViews.resize(m_swapchainImages.size());
  for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = m_swapchainImages[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = m_swapchainFormat;
    view.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_context->GetDevice(), &view, nullptr,
                          &m_swapchainImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create swapchain image view");
    }
  }

  // (Re)create per-image render-finished semaphores to avoid reuse hazards with
  // presentation.
  VkDevice device = m_context->GetDevice();
  for (auto sem : m_renderFinishedPerImage) {
    if (device != VK_NULL_HANDLE && sem != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, sem, nullptr);
    }
  }
  m_renderFinishedPerImage.clear();

  VkSemaphoreCreateInfo sem{};
  sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  m_renderFinishedPerImage.resize(m_swapchainImages.size());
  for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
    if (vkCreateSemaphore(device, &sem, nullptr,
                          &m_renderFinishedPerImage[i]) != VK_SUCCESS) {
      throw std::runtime_error(
          "Failed to create per-image renderFinished semaphore");
    }
  }

  m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);
}

void VulkanViewport::DestroySwapchain() {
  VkDevice device = (m_context && m_context->IsInitialized())
                        ? m_context->GetDevice()
                        : VK_NULL_HANDLE;

  for (auto view : m_swapchainImageViews) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
    }
  }
  m_swapchainImageViews.clear();
  m_swapchainImages.clear();

  for (auto sem : m_renderFinishedPerImage) {
    if (sem != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, sem, nullptr);
    }
  }
  m_renderFinishedPerImage.clear();
  m_imagesInFlight.clear();

  if (device != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, m_swapchain, nullptr);
  }
  m_swapchain = VK_NULL_HANDLE;

  m_swapchainExtent = {0, 0};
}

void VulkanViewport::CreateRenderPass() {
  VkDevice device = m_context->GetDevice();
  if (m_depthFormat == VK_FORMAT_UNDEFINED) {
    m_depthFormat = FindDepthFormat(m_context->GetPhysicalDevice());
  }

  VkAttachmentDescription sceneColor{};
  sceneColor.format = m_sceneColorFormat;
  sceneColor.samples = VK_SAMPLE_COUNT_1_BIT;
  sceneColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  sceneColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  sceneColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  sceneColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  sceneColor.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  sceneColor.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference sceneColorRef{};
  sceneColorRef.attachment = 0;
  sceneColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription sceneDepth{};
  sceneDepth.format = m_depthFormat;
  sceneDepth.samples = VK_SAMPLE_COUNT_1_BIT;
  sceneDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  sceneDepth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  sceneDepth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  sceneDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  sceneDepth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  sceneDepth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference sceneDepthRef{};
  sceneDepthRef.attachment = 1;
  sceneDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription sceneSubpass{};
  sceneSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sceneSubpass.colorAttachmentCount = 1;
  sceneSubpass.pColorAttachments = &sceneColorRef;
  sceneSubpass.pDepthStencilAttachment = &sceneDepthRef;

  VkSubpassDependency sceneDepBegin{};
  sceneDepBegin.srcSubpass = VK_SUBPASS_EXTERNAL;
  sceneDepBegin.dstSubpass = 0;
  sceneDepBegin.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  sceneDepBegin.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  sceneDepBegin.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  sceneDepBegin.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkSubpassDependency sceneDepEnd{};
  sceneDepEnd.srcSubpass = 0;
  sceneDepEnd.dstSubpass = VK_SUBPASS_EXTERNAL;
  sceneDepEnd.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  sceneDepEnd.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  sceneDepEnd.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  sceneDepEnd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  VkRenderPassCreateInfo sceneRp{};
  sceneRp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  std::array<VkAttachmentDescription, 2> sceneAttachments = {sceneColor,
                                                             sceneDepth};
  sceneRp.attachmentCount = static_cast<uint32_t>(sceneAttachments.size());
  sceneRp.pAttachments = sceneAttachments.data();
  sceneRp.subpassCount = 1;
  sceneRp.pSubpasses = &sceneSubpass;
  std::array<VkSubpassDependency, 2> sceneDeps = {sceneDepBegin, sceneDepEnd};
  sceneRp.dependencyCount = static_cast<uint32_t>(sceneDeps.size());
  sceneRp.pDependencies = sceneDeps.data();

  if (vkCreateRenderPass(device, &sceneRp, nullptr, &m_sceneRenderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create scene render pass");
  }

  VkAttachmentDescription postColor{};
  postColor.format = m_swapchainFormat;
  postColor.samples = VK_SAMPLE_COUNT_1_BIT;
  postColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  postColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  postColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  postColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  postColor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  postColor.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference postColorRef{};
  postColorRef.attachment = 0;
  postColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription postSubpass{};
  postSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  postSubpass.colorAttachmentCount = 1;
  postSubpass.pColorAttachments = &postColorRef;

  VkSubpassDependency postDep{};
  postDep.srcSubpass = VK_SUBPASS_EXTERNAL;
  postDep.dstSubpass = 0;
  postDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  postDep.srcAccessMask = 0;
  postDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  postDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo postRp{};
  postRp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  postRp.attachmentCount = 1;
  postRp.pAttachments = &postColor;
  postRp.subpassCount = 1;
  postRp.pSubpasses = &postSubpass;
  postRp.dependencyCount = 1;
  postRp.pDependencies = &postDep;

  if (vkCreateRenderPass(device, &postRp, nullptr, &m_postProcessRenderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create postprocess render pass");
  }

  VkAttachmentDescription pickColor{};
  pickColor.format = m_pickingFormat;
  pickColor.samples = VK_SAMPLE_COUNT_1_BIT;
  pickColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  pickColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  pickColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  pickColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  pickColor.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  pickColor.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference pickColorRef{};
  pickColorRef.attachment = 0;
  pickColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription pickDepth = sceneDepth;
  VkAttachmentReference pickDepthRef = sceneDepthRef;

  VkSubpassDescription pickSubpass = sceneSubpass;
  pickSubpass.pColorAttachments = &pickColorRef;
  pickSubpass.pDepthStencilAttachment = &pickDepthRef;

  VkSubpassDependency pickDep = sceneDepBegin;
  pickDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  pickDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  pickDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo pickRp{};
  pickRp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  std::array<VkAttachmentDescription, 2> pickAttachments = {pickColor,
                                                            pickDepth};
  pickRp.attachmentCount = static_cast<uint32_t>(pickAttachments.size());
  pickRp.pAttachments = pickAttachments.data();
  pickRp.subpassCount = 1;
  pickRp.pSubpasses = &pickSubpass;
  pickRp.dependencyCount = 1;
  pickRp.pDependencies = &pickDep;

  if (vkCreateRenderPass(device, &pickRp, nullptr, &m_pickingRenderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create picking render pass");
  }
}

void VulkanViewport::CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding ubo{};
  ubo.binding = 0;
  ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo.descriptorCount = 1;
  ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = 1;
  info.pBindings = &ubo;

  if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &info, nullptr,
                                  &m_descriptorSetLayout) != VK_SUCCESS) {
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

  if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &texInfo, nullptr,
                                  &m_textureDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create texture descriptor set layout");
  }

  VkDescriptorSetLayoutBinding sceneSampler{};
  sceneSampler.binding = 0;
  sceneSampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sceneSampler.descriptorCount = 1;
  sceneSampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding pickSampler = sceneSampler;
  pickSampler.binding = 1;

  std::array<VkDescriptorSetLayoutBinding, 2> postBindings = {sceneSampler,
                                                              pickSampler};
  VkDescriptorSetLayoutCreateInfo postInfo{};
  postInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  postInfo.bindingCount = static_cast<uint32_t>(postBindings.size());
  postInfo.pBindings = postBindings.data();

  if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &postInfo, nullptr,
                                  &m_postProcessDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create postprocess descriptor set layout");
  }
}

void VulkanViewport::CreateMeshBuffers() {
  const std::array<Vertex, 4> vertices = {
      Vertex{{-0.5f, -0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 0.2f, 0.2f, 1.0f},
             {0.0f, 0.0f}},
      Vertex{{0.5f, -0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {0.2f, 1.0f, 0.2f, 1.0f},
             {1.0f, 0.0f}},
      Vertex{{0.5f, 0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {0.2f, 0.2f, 1.0f, 1.0f},
             {1.0f, 1.0f}},
      Vertex{{-0.5f, 0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 1.0f, 0.2f, 1.0f},
             {0.0f, 1.0f}},
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
  try {
    CreateBuffer(gpu, device, vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingVertexBuffer, stagingVertexMemory);

    void *vData = nullptr;
    vkMapMemory(device, stagingVertexMemory, 0, vertexSize, 0, &vData);
    std::memcpy(vData, vertices.data(), static_cast<size_t>(vertexSize));
    vkUnmapMemory(device, stagingVertexMemory);

    CreateBuffer(
        gpu, device, vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexMemory);
    CopyBuffer(stagingVertexBuffer, m_vertexBuffer, vertexSize);

    vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
    vkFreeMemory(device, stagingVertexMemory, nullptr);
    stagingVertexBuffer = VK_NULL_HANDLE;
    stagingVertexMemory = VK_NULL_HANDLE;

    CreateBuffer(gpu, device, indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingIndexBuffer, stagingIndexMemory);

    void *iData = nullptr;
    vkMapMemory(device, stagingIndexMemory, 0, indexSize, 0, &iData);
    std::memcpy(iData, indices.data(), static_cast<size_t>(indexSize));
    vkUnmapMemory(device, stagingIndexMemory);

    CreateBuffer(
        gpu, device, indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexMemory);
    CopyBuffer(stagingIndexBuffer, m_indexBuffer, indexSize);

    vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
    vkFreeMemory(device, stagingIndexMemory, nullptr);
    stagingIndexBuffer = VK_NULL_HANDLE;
    stagingIndexMemory = VK_NULL_HANDLE;
  } catch (...) {
    if (stagingVertexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
    }
    if (stagingVertexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, stagingVertexMemory, nullptr);
    }
    if (stagingIndexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
    }
    if (stagingIndexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, stagingIndexMemory, nullptr);
    }
    throw;
  }
  m_defaultIndexCount = static_cast<uint32_t>(indices.size());

  m_iconMesh = {};
  const std::array<Vertex, 4> iconVertices = {
      Vertex{{-0.5f, -0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 1.0f, 1.0f, 1.0f},
             {0.0f, 0.0f}},
      Vertex{{0.5f, -0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 1.0f, 1.0f, 1.0f},
             {1.0f, 0.0f}},
      Vertex{{0.5f, 0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 1.0f, 1.0f, 1.0f},
             {1.0f, 1.0f}},
      Vertex{{-0.5f, 0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 1.0f, 1.0f, 1.0f},
             {0.0f, 1.0f}},
  };
  const std::array<uint32_t, 6> iconIndices = {0, 1, 2, 2, 3, 0};

  VkBuffer iconStagingVertex = VK_NULL_HANDLE;
  VkDeviceMemory iconStagingVertexMemory = VK_NULL_HANDLE;
  VkBuffer iconStagingIndex = VK_NULL_HANDLE;
  VkDeviceMemory iconStagingIndexMemory = VK_NULL_HANDLE;
  try {
    const VkDeviceSize iconVertexSize = sizeof(iconVertices);
    const VkDeviceSize iconIndexSize = sizeof(iconIndices);

    CreateBuffer(gpu, device, iconVertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 iconStagingVertex, iconStagingVertexMemory);

    void *vIconData = nullptr;
    vkMapMemory(device, iconStagingVertexMemory, 0, iconVertexSize, 0,
                &vIconData);
    std::memcpy(vIconData, iconVertices.data(),
                static_cast<size_t>(iconVertexSize));
    vkUnmapMemory(device, iconStagingVertexMemory);

    CreateBuffer(gpu, device, iconVertexSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_iconMesh.vertexBuffer,
                 m_iconMesh.vertexMemory);
    CopyBuffer(iconStagingVertex, m_iconMesh.vertexBuffer, iconVertexSize);

    CreateBuffer(gpu, device, iconIndexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 iconStagingIndex, iconStagingIndexMemory);

    void *iIconData = nullptr;
    vkMapMemory(device, iconStagingIndexMemory, 0, iconIndexSize, 0,
                &iIconData);
    std::memcpy(iIconData, iconIndices.data(),
                static_cast<size_t>(iconIndexSize));
    vkUnmapMemory(device, iconStagingIndexMemory);

    CreateBuffer(gpu, device, iconIndexSize,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_iconMesh.indexBuffer,
                 m_iconMesh.indexMemory);
    CopyBuffer(iconStagingIndex, m_iconMesh.indexBuffer, iconIndexSize);
    m_iconMesh.indexCount = static_cast<uint32_t>(iconIndices.size());
  } catch (...) {
    if (iconStagingVertex != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, iconStagingVertex, nullptr);
    }
    if (iconStagingVertexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, iconStagingVertexMemory, nullptr);
    }
    if (iconStagingIndex != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, iconStagingIndex, nullptr);
    }
    if (iconStagingIndexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, iconStagingIndexMemory, nullptr);
    }
    throw;
  }

  if (iconStagingVertex != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, iconStagingVertex, nullptr);
  }
  if (iconStagingVertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, iconStagingVertexMemory, nullptr);
  }
  if (iconStagingIndex != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, iconStagingIndex, nullptr);
  }
  if (iconStagingIndexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, iconStagingIndexMemory, nullptr);
  }
}

void VulkanViewport::CreateLineBuffers() {
  const float gridHalf = 10.0f;
  const float step = 1.0f;
  const int gridCount = static_cast<int>(gridHalf / step);

  const float normal[3] = {0.0f, 0.0f, 1.0f};
  std::vector<Vertex> vertices;
  vertices.reserve(static_cast<size_t>((gridCount * 2 + 1) * 4 + 4));

  for (int i = -gridCount; i <= gridCount; ++i) {
    const float t = static_cast<float>(i) * step;
    const float color[4] = {0.35f, 0.35f, 0.35f, 1.0f};

    vertices.push_back(Vertex{{-gridHalf, t, 0.0f},
                              {normal[0], normal[1], normal[2]},
                              {color[0], color[1], color[2], color[3]},
                              {0.0f, 0.0f}});
    vertices.push_back(Vertex{{gridHalf, t, 0.0f},
                              {normal[0], normal[1], normal[2]},
                              {color[0], color[1], color[2], color[3]},
                              {1.0f, 0.0f}});

    vertices.push_back(Vertex{{t, -gridHalf, 0.0f},
                              {normal[0], normal[1], normal[2]},
                              {color[0], color[1], color[2], color[3]},
                              {0.0f, 0.0f}});
    vertices.push_back(Vertex{{t, gridHalf, 0.0f},
                              {normal[0], normal[1], normal[2]},
                              {color[0], color[1], color[2], color[3]},
                              {1.0f, 0.0f}});
  }

  // Axis lines (X = red, Y = green).
  vertices.push_back(Vertex{{-gridHalf, 0.0f, 0.0f},
                            {normal[0], normal[1], normal[2]},
                            {0.85f, 0.20f, 0.20f, 1.0f},
                            {0.0f, 0.0f}});
  vertices.push_back(Vertex{{gridHalf, 0.0f, 0.0f},
                            {normal[0], normal[1], normal[2]},
                            {0.85f, 0.20f, 0.20f, 1.0f},
                            {1.0f, 0.0f}});
  vertices.push_back(Vertex{{0.0f, -gridHalf, 0.0f},
                            {normal[0], normal[1], normal[2]},
                            {0.20f, 0.85f, 0.20f, 1.0f},
                            {0.0f, 0.0f}});
  vertices.push_back(Vertex{{0.0f, gridHalf, 0.0f},
                            {normal[0], normal[1], normal[2]},
                            {0.20f, 0.85f, 0.20f, 1.0f},
                            {1.0f, 0.0f}});

  m_lineVertexCount = static_cast<uint32_t>(vertices.size());

  VkDevice device = m_context->GetDevice();
  VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

  CreateBuffer(gpu, device, sizeof(Vertex) * vertices.size(),
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               m_lineVertexBuffer, m_lineVertexMemory);

  void *vData = nullptr;
  vkMapMemory(device, m_lineVertexMemory, 0, sizeof(Vertex) * vertices.size(),
              0, &vData);
  std::memcpy(vData, vertices.data(), sizeof(Vertex) * vertices.size());
  vkUnmapMemory(device, m_lineVertexMemory);

  const size_t maxSelectionVerts = 128;
  CreateBuffer(gpu, device, sizeof(Vertex) * maxSelectionVerts,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               m_selectionVertexBuffer, m_selectionVertexMemory);
  m_selectionVertexCount = 0;

  // Light gizmo buffer: support multiple lights with line gizmos.
  const size_t maxLightGizmoVerts = kMaxLights * 96;
  CreateBuffer(gpu, device, sizeof(Vertex) * maxLightGizmoVerts,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               m_lightGizmoVertexBuffer, m_lightGizmoVertexMemory);
  m_lightGizmoVertexCount = 0;
}

void VulkanViewport::CreateSceneResources() {
  if (!m_context || !m_context->IsInitialized()) {
    return;
  }

  DestroySceneResources();

  if (m_swapchainExtent.width == 0 || m_swapchainExtent.height == 0) {
    return;
  }

  VkDevice device = m_context->GetDevice();
  VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

  if (m_depthFormat == VK_FORMAT_UNDEFINED) {
    m_depthFormat = FindDepthFormat(gpu);
  }
  if (m_sceneColorFormat == VK_FORMAT_UNDEFINED) {
    m_sceneColorFormat = FindSceneColorFormat(gpu);
  }

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    CreateImage(gpu, device, m_swapchainExtent.width, m_swapchainExtent.height,
                m_sceneColorFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_sceneColorImages[i],
                m_sceneColorMemories[i]);

    m_sceneColorViews[i] =
        CreateImageView(device, m_sceneColorImages[i], m_sceneColorFormat,
                        VK_IMAGE_ASPECT_COLOR_BIT);
    TransitionImageLayout(m_sceneColorImages[i], m_sceneColorFormat,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    CreateImage(gpu, device, m_swapchainExtent.width, m_swapchainExtent.height,
                m_depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_sceneDepthImages[i],
                m_sceneDepthMemories[i]);

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(m_depthFormat)) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    m_sceneDepthViews[i] =
        CreateImageView(device, m_sceneDepthImages[i], m_depthFormat, aspect);
    TransitionImageLayout(m_sceneDepthImages[i], m_depthFormat,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }
}

void VulkanViewport::CreatePickingResources() {
  if (!m_context || !m_context->IsInitialized()) {
    return;
  }

  DestroyPickingResources();

  if (m_swapchainExtent.width == 0 || m_swapchainExtent.height == 0) {
    return;
  }

  VkDevice device = m_context->GetDevice();
  VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

  if (m_pickingFormat == VK_FORMAT_UNDEFINED) {
    const auto pickInfo = FindPickingFormat(gpu);
    m_pickingFormat = pickInfo.format;
    m_pickingFormatIsUint = pickInfo.isUint;
  }
  if (m_depthFormat == VK_FORMAT_UNDEFINED) {
    m_depthFormat = FindDepthFormat(gpu);
  }

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    CreateImage(gpu, device, m_swapchainExtent.width, m_swapchainExtent.height,
                m_pickingFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pickingImages[i],
                m_pickingMemories[i]);

    m_pickingViews[i] = CreateImageView(
        device, m_pickingImages[i], m_pickingFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    TransitionImageLayout(m_pickingImages[i], m_pickingFormat,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    CreateImage(gpu, device, m_swapchainExtent.width, m_swapchainExtent.height,
                m_depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pickingDepthImages[i],
                m_pickingDepthMemories[i]);

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(m_depthFormat)) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    m_pickingDepthViews[i] =
        CreateImageView(device, m_pickingDepthImages[i], m_depthFormat, aspect);
    TransitionImageLayout(m_pickingDepthImages[i], m_depthFormat,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    CreateBuffer(gpu, device, sizeof(uint32_t),
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_pickingReadbackBuffers[i], m_pickingReadbackMemories[i]);
  }
}

void VulkanViewport::CreateUniformBuffers() {
  VkDevice device = m_context->GetDevice();
  VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

  const VkDeviceSize bufferSize = sizeof(FrameUniformObject);

  m_uniformMapped.fill(nullptr);

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    CreateBuffer(gpu, device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_uniformBuffers[i], m_uniformMemories[i]);

    vkMapMemory(device, m_uniformMemories[i], 0, bufferSize, 0,
                &m_uniformMapped[i]);
  }
}

void VulkanViewport::CreateDescriptorPoolAndSets() {
  VkDevice device = m_context->GetDevice();

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (m_descriptorPools[i] != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, m_descriptorPools[i], nullptr);
      m_descriptorPools[i] = VK_NULL_HANDLE;
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;

    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool.pPoolSizes = poolSizes.data();
    pool.maxSets = 2;

    if (vkCreateDescriptorPool(device, &pool, nullptr, &m_descriptorPools[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create descriptor pool");
    }

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_descriptorPools[i];
    alloc.descriptorSetCount = 1;
    VkDescriptorSetLayout uboLayout = m_descriptorSetLayout;
    alloc.pSetLayouts = &uboLayout;
    if (vkAllocateDescriptorSets(device, &alloc, &m_descriptorSets[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate UBO descriptor set");
    }

    VkDescriptorSetLayout postLayout = m_postProcessDescriptorSetLayout;
    alloc.pSetLayouts = &postLayout;
    if (vkAllocateDescriptorSets(
            device, &alloc, &m_postProcessDescriptorSets[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate postprocess descriptor set");
    }

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

void VulkanViewport::CreateTextureDescriptorPool() {
  VkDevice device = m_context->GetDevice();
  for (auto pool : m_textureDescriptorPools) {
    if (pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, pool, nullptr);
    }
  }
  m_textureDescriptorPools.clear();
  m_activeTextureDescriptorPool = 0;
  m_textureDescriptorPools.push_back(CreateTextureDescriptorPoolInternal());
}

VkDescriptorPool VulkanViewport::CreateTextureDescriptorPoolInternal() {
  VkDescriptorPool poolHandle = VK_NULL_HANDLE;
  if (!m_context || !m_context->IsInitialized()) {
    return poolHandle;
  }

  VkDevice device = m_context->GetDevice();
  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = kMaxTextureDescriptors;

  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool.poolSizeCount = 1;
  pool.pPoolSizes = &poolSize;
  pool.maxSets = kMaxTextureDescriptors;

  if (vkCreateDescriptorPool(device, &pool, nullptr, &poolHandle) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create texture descriptor pool");
  }
  return poolHandle;
}

void VulkanViewport::CreateTextureResources() {
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
  sampler.minLod = 0.0f;
  sampler.maxLod = 0.0f;

  if (m_context && m_context->IsSamplerAnisotropyEnabled()) {
    sampler.anisotropyEnable = VK_TRUE;
    sampler.maxAnisotropy =
        std::min(16.0f, m_context->GetMaxSamplerAnisotropy());
  }

  if (vkCreateSampler(device, &sampler, nullptr, &m_textureSampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create texture sampler");
  }

  VkSamplerCreateInfo postSampler = sampler;
  if (m_pickingFormatIsUint) {
    postSampler.magFilter = VK_FILTER_NEAREST;
    postSampler.minFilter = VK_FILTER_NEAREST;
    postSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
  postSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  postSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  postSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (vkCreateSampler(device, &postSampler, nullptr, &m_postProcessSampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create postprocess sampler");
  }

  const unsigned char white[4] = {255, 255, 255, 255};
  m_defaultTexture = CreateTextureFromPixels(white, 1, 1);
}

void VulkanViewport::CreatePipeline() {
  auto vert = ReadFileBinary(ShaderPath("viewport_triangle.vert.spv"));
  auto frag = ReadFileBinary(ShaderPath("viewport_triangle.frag.spv"));
  auto pickFrag = ReadFileBinary(ShaderPath("viewport_picking.frag.spv"));
  auto pickFragUint =
      ReadFileBinary(ShaderPath("viewport_picking_uint.frag.spv"));
  auto postVert = ReadFileBinary(ShaderPath("viewport_postprocess.vert.spv"));
  auto postFrag = ReadFileBinary(ShaderPath("viewport_postprocess.frag.spv"));
  auto postFragUint =
      ReadFileBinary(ShaderPath("viewport_postprocess_uint.frag.spv"));

  VkShaderModule vertModule = CreateShaderModule(vert);
  VkShaderModule fragModule = CreateShaderModule(frag);
  VkShaderModule pickFragModule = CreateShaderModule(pickFrag);
  VkShaderModule pickFragUintModule = CreateShaderModule(pickFragUint);
  VkShaderModule postVertModule = CreateShaderModule(postVert);
  VkShaderModule postFragModule = CreateShaderModule(postFrag);
  VkShaderModule postFragUintModule = CreateShaderModule(postFragUint);

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
  vertexInput.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attrs.size());
  vertexInput.pVertexAttributeDescriptions = attrs.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
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
  // Viewport transform can flip the winding; disable culling for this debug
  // viewport.
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
  raster.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo msaa{};
  msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blendAttach{};
  blendAttach.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
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
  pushRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(InstancePushConstants);

  std::array<VkDescriptorSetLayout, 2> setLayouts = {
      m_descriptorSetLayout, m_textureDescriptorSetLayout};

  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  layout.pSetLayouts = setLayouts.data();
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &pushRange;

  if (vkCreatePipelineLayout(m_context->GetDevice(), &layout, nullptr,
                             &m_pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout");
  }

  VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                VK_DYNAMIC_STATE_SCISSOR};
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
  pipe.renderPass = m_sceneRenderPass;
  pipe.subpass = 0;

  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &pipe, nullptr, &m_pipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create graphics pipeline");
  }

  VkPipelineInputAssemblyStateCreateInfo lineInput = inputAssembly;
  lineInput.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

  VkGraphicsPipelineCreateInfo linePipe = pipe;
  linePipe.pInputAssemblyState = &lineInput;
  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &linePipe, nullptr,
                                &m_linePipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create line pipeline");
  }

  VkPipelineDepthStencilStateCreateInfo overlayDepth = depth;
  overlayDepth.depthTestEnable = VK_FALSE;
  overlayDepth.depthWriteEnable = VK_FALSE;

  VkGraphicsPipelineCreateInfo overlayPipe = linePipe;
  overlayPipe.pDepthStencilState = &overlayDepth;
  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &overlayPipe, nullptr,
                                &m_overlayPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create overlay pipeline");
  }

  VkPipelineShaderStageCreateInfo pickFs = fs;
  pickFs.module = pickFragModule;
  VkPipelineShaderStageCreateInfo pickStages[] = {vs, pickFs};
  VkGraphicsPipelineCreateInfo pickPipe = pipe;
  pickPipe.pStages = pickStages;
  pickPipe.renderPass = m_pickingRenderPass;
  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &pickPipe, nullptr,
                                &m_pickingPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create picking pipeline");
  }

  VkPipelineShaderStageCreateInfo pickFsUint = fs;
  pickFsUint.module = pickFragUintModule;
  VkPipelineShaderStageCreateInfo pickStagesUint[] = {vs, pickFsUint};
  VkGraphicsPipelineCreateInfo pickPipeUint = pickPipe;
  pickPipeUint.pStages = pickStagesUint;
  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &pickPipeUint, nullptr,
                                &m_pickingPipelineUint) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create uint picking pipeline");
  }

  VkPipelineShaderStageCreateInfo postVs{};
  postVs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  postVs.stage = VK_SHADER_STAGE_VERTEX_BIT;
  postVs.module = postVertModule;
  postVs.pName = "main";

  VkPipelineShaderStageCreateInfo postFs{};
  postFs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  postFs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  postFs.module = postFragModule;
  postFs.pName = "main";

  VkPipelineShaderStageCreateInfo postStages[] = {postVs, postFs};

  VkPipelineVertexInputStateCreateInfo postVertexInput{};
  postVertexInput.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  postVertexInput.vertexBindingDescriptionCount = 0;
  postVertexInput.vertexAttributeDescriptionCount = 0;

  VkPipelineDepthStencilStateCreateInfo postDepth{};
  postDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  postDepth.depthTestEnable = VK_FALSE;
  postDepth.depthWriteEnable = VK_FALSE;
  postDepth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
  postDepth.stencilTestEnable = VK_FALSE;

  std::array<VkDescriptorSetLayout, 2> postLayouts = {
      m_descriptorSetLayout, m_postProcessDescriptorSetLayout};
  VkPipelineLayoutCreateInfo postLayout{};
  postLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  postLayout.setLayoutCount = static_cast<uint32_t>(postLayouts.size());
  postLayout.pSetLayouts = postLayouts.data();

  if (vkCreatePipelineLayout(m_context->GetDevice(), &postLayout, nullptr,
                             &m_postProcessPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create postprocess pipeline layout");
  }

  VkGraphicsPipelineCreateInfo postPipe{};
  postPipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  postPipe.stageCount = 2;
  postPipe.pStages = postStages;
  postPipe.pVertexInputState = &postVertexInput;
  postPipe.pInputAssemblyState = &inputAssembly;
  postPipe.pViewportState = &viewportState;
  postPipe.pRasterizationState = &raster;
  postPipe.pMultisampleState = &msaa;
  postPipe.pColorBlendState = &blend;
  postPipe.pDepthStencilState = &postDepth;
  postPipe.pDynamicState = &dyn;
  postPipe.layout = m_postProcessPipelineLayout;
  postPipe.renderPass = m_postProcessRenderPass;
  postPipe.subpass = 0;

  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &postPipe, nullptr,
                                &m_postProcessPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create postprocess pipeline");
  }

  VkPipelineShaderStageCreateInfo postFsUint = postFs;
  postFsUint.module = postFragUintModule;
  VkPipelineShaderStageCreateInfo postStagesUint[] = {postVs, postFsUint};
  VkGraphicsPipelineCreateInfo postPipeUint = postPipe;
  postPipeUint.pStages = postStagesUint;
  if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1,
                                &postPipeUint, nullptr,
                                &m_postProcessPipelineUint) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create uint postprocess pipeline");
  }

  vkDestroyShaderModule(m_context->GetDevice(), postFragUintModule, nullptr);
  vkDestroyShaderModule(m_context->GetDevice(), postFragModule, nullptr);
  vkDestroyShaderModule(m_context->GetDevice(), postVertModule, nullptr);
  vkDestroyShaderModule(m_context->GetDevice(), pickFragUintModule, nullptr);
  vkDestroyShaderModule(m_context->GetDevice(), pickFragModule, nullptr);
  vkDestroyShaderModule(m_context->GetDevice(), fragModule, nullptr);
  vkDestroyShaderModule(m_context->GetDevice(), vertModule, nullptr);
}

void VulkanViewport::CreateFramebuffers() {
  VkDevice device = m_context->GetDevice();

  // Ensure any previous framebuffers are gone (e.g., on resize).
  for (auto fb : m_framebuffers) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
  }
  m_framebuffers.clear();

  for (auto &fb : m_sceneFramebuffers) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
      fb = VK_NULL_HANDLE;
    }
  }
  for (auto &fb : m_pickingFramebuffers) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
      fb = VK_NULL_HANDLE;
    }
  }

  m_framebuffers.resize(m_swapchainImageViews.size());
  for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
    VkImageView attachments[] = {m_swapchainImageViews[i]};

    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = m_postProcessRenderPass;
    fb.attachmentCount = 1;
    fb.pAttachments = attachments;
    fb.width = m_swapchainExtent.width;
    fb.height = m_swapchainExtent.height;
    fb.layers = 1;

    if (vkCreateFramebuffer(device, &fb, nullptr, &m_framebuffers[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create postprocess framebuffer");
    }
  }

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    VkImageView sceneAttachments[] = {m_sceneColorViews[i],
                                      m_sceneDepthViews[i]};
    VkFramebufferCreateInfo sceneFb{};
    sceneFb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    sceneFb.renderPass = m_sceneRenderPass;
    sceneFb.attachmentCount = 2;
    sceneFb.pAttachments = sceneAttachments;
    sceneFb.width = m_swapchainExtent.width;
    sceneFb.height = m_swapchainExtent.height;
    sceneFb.layers = 1;
    if (vkCreateFramebuffer(device, &sceneFb, nullptr,
                            &m_sceneFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create scene framebuffer");
    }

    VkImageView pickAttachments[] = {m_pickingViews[i], m_pickingDepthViews[i]};
    VkFramebufferCreateInfo pickFb{};
    pickFb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    pickFb.renderPass = m_pickingRenderPass;
    pickFb.attachmentCount = 2;
    pickFb.pAttachments = pickAttachments;
    pickFb.width = m_swapchainExtent.width;
    pickFb.height = m_swapchainExtent.height;
    pickFb.layers = 1;
    if (vkCreateFramebuffer(device, &pickFb, nullptr,
                            &m_pickingFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create picking framebuffer");
    }
  }
}

void VulkanViewport::UpdatePostProcessDescriptorSets() {
  if (!m_context || !m_context->IsInitialized()) {
    return;
  }

  VkDevice device = m_context->GetDevice();
  if (m_postProcessSampler == VK_NULL_HANDLE) {
    return;
  }

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (m_postProcessDescriptorSets[i] == VK_NULL_HANDLE) {
      continue;
    }
    if (m_sceneColorViews[i] == VK_NULL_HANDLE ||
        m_pickingViews[i] == VK_NULL_HANDLE) {
      continue;
    }

    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler = m_postProcessSampler;
    sceneInfo.imageView = m_sceneColorViews[i];
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo pickInfo{};
    pickInfo.sampler = m_postProcessSampler;
    pickInfo.imageView = m_pickingViews[i];
    pickInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_postProcessDescriptorSets[i];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &sceneInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_postProcessDescriptorSets[i];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &pickInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }
}

void VulkanViewport::CreateCommandPoolAndBuffers() {
  VkCommandPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = m_context->GetGraphicsQueueFamilyIndex();

  if (vkCreateCommandPool(m_context->GetDevice(), &pool, nullptr,
                          &m_commandPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool");
  }

  m_commandBuffers.resize(kMaxFramesInFlight);

  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = m_commandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

  if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc,
                               m_commandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate command buffers");
  }
}

void VulkanViewport::RecordCommandBuffer(
    uint32_t imageIndex, const std::vector<DrawInstance> &instances) {
  VkCommandBuffer cb = m_commandBuffers[m_frameIndex];

  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS) {
    throw std::runtime_error("vkBeginCommandBuffer failed");
  }

  if (m_timestampsSupported && m_queryPools[m_frameIndex] != VK_NULL_HANDLE) {
    vkCmdResetQueryPool(cb, m_queryPools[m_frameIndex], 0, kPassCount * 2);
  }

  auto recordPass = [&](uint32_t passIndex, auto &&fn) {
    if (m_timestampsSupported && m_queryPools[m_frameIndex] != VK_NULL_HANDLE) {
      vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          m_queryPools[m_frameIndex], passIndex * 2);
    }
    const auto cpuStart = std::chrono::steady_clock::now();
    fn();
    const auto cpuEnd = std::chrono::steady_clock::now();
    m_frameStats[m_frameIndex].passes[passIndex].cpuMs =
        std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
    if (m_timestampsSupported && m_queryPools[m_frameIndex] != VK_NULL_HANDLE) {
      vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          m_queryPools[m_frameIndex], passIndex * 2 + 1);
    }
  };

  recordPass(0, [&]() { RecordOpaquePass(cb, instances); });

  const bool needsPicking =
      m_pendingPick.pending || m_debugViewMode == DebugViewMode::EntityId;
  recordPass(1, [&]() {
    if (needsPicking) {
      RecordPickingPass(cb, instances);
    }
  });

  recordPass(2, [&]() { RecordPostProcessPass(cb, imageIndex); });
  recordPass(3, [&]() { RecordOverlayPass(cb); });

  if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
    throw std::runtime_error("vkEndCommandBuffer failed");
  }
}

void VulkanViewport::RecordOpaquePass(
    VkCommandBuffer cb, const std::vector<DrawInstance> &instances) {
  VkClearValue clear[2]{};
  clear[0].color = {{0.02f, 0.02f, 0.02f, 1.0f}};
  clear[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = m_sceneRenderPass;
  rp.framebuffer = m_sceneFramebuffers[m_frameIndex];
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
  if (m_defaultTexture.descriptorSet != VK_NULL_HANDLE) {
    VkDescriptorSet sets[] = {uboSet, m_defaultTexture.descriptorSet};
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 2, sets, 0, nullptr);
    boundTextureSet = m_defaultTexture.descriptorSet;
  }

  InstancePushConstants baseConstants{};
  Mat4Identity(baseConstants.model);
  baseConstants.color[0] = 1.0f;
  baseConstants.color[1] = 1.0f;
  baseConstants.color[2] = 1.0f;
  baseConstants.color[3] = 1.0f;

  if (m_linePipeline != VK_NULL_HANDLE &&
      m_lineVertexBuffer != VK_NULL_HANDLE && m_lineVertexCount > 0) {
    baseConstants.flags = kInstanceFlagUnlit;
    vkCmdPushConstants(cb, m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(InstancePushConstants), &baseConstants);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
    vkCmdBindVertexBuffers(cb, 0, 1, &m_lineVertexBuffer, offsets);
    vkCmdDraw(cb, m_lineVertexCount, 1, 0, 0);
  }

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

  if (!instances.empty()) {
    bool hasBoundMesh = false;
    VkBuffer boundVertex = VK_NULL_HANDLE;
    VkBuffer boundIndex = VK_NULL_HANDLE;
    for (const auto &instance : instances) {
      const GpuTexture *texture = ResolveTexture(instance.textureId);
      VkDescriptorSet textureSet =
          (texture && texture->descriptorSet != VK_NULL_HANDLE)
              ? texture->descriptorSet
              : m_defaultTexture.descriptorSet;

      if (textureSet != VK_NULL_HANDLE && textureSet != boundTextureSet) {
        VkDescriptorSet sets[] = {uboSet, textureSet};
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout, 0, 2, sets, 0, nullptr);
        boundTextureSet = textureSet;
      }

      const GpuMesh *mesh = ResolveMesh(instance.meshId);
      VkBuffer vertexBuffer = m_vertexBuffer;
      VkBuffer indexBuffer = m_indexBuffer;
      uint32_t indexCount = m_defaultIndexCount;

      if (mesh && mesh->vertexBuffer != VK_NULL_HANDLE &&
          mesh->indexBuffer != VK_NULL_HANDLE && mesh->indexCount > 0) {
        vertexBuffer = mesh->vertexBuffer;
        indexBuffer = mesh->indexBuffer;
        indexCount = mesh->indexCount;
      }

      if (!hasBoundMesh || vertexBuffer != boundVertex ||
          indexBuffer != boundIndex) {
        vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cb, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        hasBoundMesh = true;
        boundVertex = vertexBuffer;
        boundIndex = indexBuffer;
      }

      vkCmdPushConstants(cb, m_pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(InstancePushConstants), &instance.constants);
      vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
    }
  } else {
    InstancePushConstants defaultQuad{};
    Mat4Identity(defaultQuad.model);
    float scale[16];
    Mat4Scale(scale, 0.8f, 0.8f, 1.0f);
    std::memcpy(defaultQuad.model, scale, sizeof(scale));
    defaultQuad.color[0] = 0.95f;
    defaultQuad.color[1] = 0.30f;
    defaultQuad.color[2] = 0.70f;
    defaultQuad.color[3] = 1.0f;

    if (m_defaultTexture.descriptorSet != VK_NULL_HANDLE) {
      VkDescriptorSet sets[] = {uboSet, m_defaultTexture.descriptorSet};
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_pipelineLayout, 0, 2, sets, 0, nullptr);
    }

    vkCmdPushConstants(cb, m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(InstancePushConstants), &defaultQuad);
    vkCmdBindVertexBuffers(cb, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cb, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cb, m_defaultIndexCount, 1, 0, 0, 0);
  }

  if (m_overlayPipeline != VK_NULL_HANDLE &&
      m_selectionVertexBuffer != VK_NULL_HANDLE && m_selectionVertexCount > 0) {
    baseConstants.flags = kInstanceFlagUnlit;
    vkCmdPushConstants(cb, m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(InstancePushConstants), &baseConstants);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_overlayPipeline);
    if (m_defaultTexture.descriptorSet != VK_NULL_HANDLE &&
        boundTextureSet != m_defaultTexture.descriptorSet) {
      VkDescriptorSet sets[] = {uboSet, m_defaultTexture.descriptorSet};
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_pipelineLayout, 0, 2, sets, 0, nullptr);
      boundTextureSet = m_defaultTexture.descriptorSet;
    }
    vkCmdBindVertexBuffers(cb, 0, 1, &m_selectionVertexBuffer, offsets);
    vkCmdDraw(cb, m_selectionVertexCount, 1, 0, 0);
  }

  if (m_overlayPipeline != VK_NULL_HANDLE &&
      m_lightGizmoVertexBuffer != VK_NULL_HANDLE &&
      m_lightGizmoVertexCount > 0) {
    baseConstants.flags = kInstanceFlagUnlit;
    vkCmdPushConstants(cb, m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(InstancePushConstants), &baseConstants);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_overlayPipeline);
    vkCmdBindVertexBuffers(cb, 0, 1, &m_lightGizmoVertexBuffer, offsets);
    vkCmdDraw(cb, m_lightGizmoVertexCount, 1, 0, 0);
  }

  vkCmdEndRenderPass(cb);
}

void VulkanViewport::RecordPickingPass(
    VkCommandBuffer cb, const std::vector<DrawInstance> &instances) {
  if (instances.empty()) {
    if (!m_pendingPick.pending && m_debugViewMode != DebugViewMode::EntityId) {
      return;
    }
  }

  VkClearValue clear[2]{};
  clear[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
  clear[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = m_pickingRenderPass;
  rp.framebuffer = m_pickingFramebuffers[m_frameIndex];
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
  VkDescriptorSet textureSet = m_defaultTexture.descriptorSet;
  if (textureSet != VK_NULL_HANDLE) {
    VkDescriptorSet sets[] = {uboSet, textureSet};
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 2, sets, 0, nullptr);
  }

  VkPipeline pickPipeline =
      m_pickingFormatIsUint ? m_pickingPipelineUint : m_pickingPipeline;
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pickPipeline);

  bool hasBoundMesh = false;
  VkBuffer boundVertex = VK_NULL_HANDLE;
  VkBuffer boundIndex = VK_NULL_HANDLE;
  for (const auto &instance : instances) {
    const GpuMesh *mesh = ResolveMesh(instance.meshId);
    VkBuffer vertexBuffer = m_vertexBuffer;
    VkBuffer indexBuffer = m_indexBuffer;
    uint32_t indexCount = m_defaultIndexCount;

    if (mesh && mesh->vertexBuffer != VK_NULL_HANDLE &&
        mesh->indexBuffer != VK_NULL_HANDLE && mesh->indexCount > 0) {
      vertexBuffer = mesh->vertexBuffer;
      indexBuffer = mesh->indexBuffer;
      indexCount = mesh->indexCount;
    }

    if (!hasBoundMesh || vertexBuffer != boundVertex ||
        indexBuffer != boundIndex) {
      vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, offsets);
      vkCmdBindIndexBuffer(cb, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
      hasBoundMesh = true;
      boundVertex = vertexBuffer;
      boundIndex = indexBuffer;
    }

    vkCmdPushConstants(cb, m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(InstancePushConstants), &instance.constants);
    vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(cb);

  const bool needsSampling = (m_debugViewMode == DebugViewMode::EntityId);
  const bool needsReadback = m_pendingPick.pending;

  if (needsReadback || needsSampling) {
    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toTransfer.newLayout = needsReadback
                               ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = m_pickingImages[m_frameIndex];
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask =
        needsReadback ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         needsReadback ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                       : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toTransfer);
  }

  if (needsReadback &&
      m_pickingReadbackBuffers[m_frameIndex] != VK_NULL_HANDLE) {
    uint32_t pickX = m_pendingPick.x;
    uint32_t pickY = m_pendingPick.y;
    if (m_swapchainExtent.width > 0) {
      pickX = std::min(pickX, m_swapchainExtent.width - 1);
    }
    if (m_swapchainExtent.height > 0) {
      pickY = std::min(pickY, m_swapchainExtent.height - 1);
    }
    if (m_pickFlipY && m_swapchainExtent.height > 0) {
      pickY = (m_swapchainExtent.height - 1) - pickY;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(pickX),
                          static_cast<int32_t>(pickY), 0};
    region.imageExtent = {1, 1, 1};
    vkCmdCopyImageToBuffer(cb, m_pickingImages[m_frameIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_pickingReadbackBuffers[m_frameIndex], 1, &region);

    VkImageMemoryBarrier toSample{};
    toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = m_pickingImages[m_frameIndex];
    toSample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toSample.subresourceRange.baseMipLevel = 0;
    toSample.subresourceRange.levelCount = 1;
    toSample.subresourceRange.baseArrayLayer = 0;
    toSample.subresourceRange.layerCount = 1;
    toSample.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &toSample);

    m_pickReadbacks[m_frameIndex].inFlight = true;
    m_pickReadbacks[m_frameIndex].x = m_pendingPick.x;
    m_pickReadbacks[m_frameIndex].y = m_pendingPick.y;
    m_pendingPick.pending = false;
  } else if (needsSampling) {
    VkImageMemoryBarrier toSample{};
    toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSample.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = m_pickingImages[m_frameIndex];
    toSample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toSample.subresourceRange.baseMipLevel = 0;
    toSample.subresourceRange.levelCount = 1;
    toSample.subresourceRange.baseArrayLayer = 0;
    toSample.subresourceRange.layerCount = 1;
    toSample.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &toSample);
  }
}

void VulkanViewport::RecordPostProcessPass(VkCommandBuffer cb,
                                           uint32_t imageIndex) {
  VkClearValue clear{};
  clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = m_postProcessRenderPass;
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

  VkPipeline postPipeline =
      m_pickingFormatIsUint ? m_postProcessPipelineUint : m_postProcessPipeline;
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline);

  VkDescriptorSet sets[] = {m_descriptorSets[m_frameIndex],
                            m_postProcessDescriptorSets[m_frameIndex]};
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_postProcessPipelineLayout, 0, 2, sets, 0, nullptr);

  vkCmdDraw(cb, 3, 1, 0, 0);
  vkCmdEndRenderPass(cb);
}

void VulkanViewport::RecordOverlayPass(VkCommandBuffer cb) { (void)cb; }

void VulkanViewport::UpdateUniformBuffer(uint32_t frameIndex,
                                         const RenderView &view) {
  if (frameIndex >= kMaxFramesInFlight) {
    return;
  }

  const float aspect = (m_swapchainExtent.height > 0)
                           ? (static_cast<float>(m_swapchainExtent.width) /
                              static_cast<float>(m_swapchainExtent.height))
                           : 1.0f;

  float proj[16];
  float viewMat[16];
  float eyeX = 0.0f;
  float eyeY = 0.0f;
  float eyeZ = 0.0f;
  float nearPlane = 0.1f;
  float farPlane = 100.0f;
  const bool useSceneCamera = view.camera.enabled;
  if (useSceneCamera) {
    nearPlane = view.camera.nearClip;
    farPlane = view.camera.farClip;
    if (view.camera.projectionType == 1) {
      const float halfHeight =
          std::max(0.01f, view.camera.orthographicSize) * 0.5f;
      const float halfWidth = halfHeight * aspect;
      Mat4Ortho(proj, -halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane,
                farPlane);
    } else {
      const float fovRad =
          view.camera.verticalFov * (3.14159265358979323846f / 180.0f);
      Mat4Perspective(proj, fovRad, aspect, nearPlane, farPlane);
    }

    eyeX = view.camera.position[0];
    eyeY = view.camera.position[1];
    eyeZ = view.camera.position[2];
    const float center[3] = {eyeX + view.camera.forward[0],
                             eyeY + view.camera.forward[1],
                             eyeZ + view.camera.forward[2]};
    const float up[3] = {view.camera.up[0], view.camera.up[1],
                         view.camera.up[2]};
    const float eye[3] = {eyeX, eyeY, eyeZ};
    Mat4LookAt(viewMat, eye, center, up);
  } else {
    Mat4Perspective(proj, 60.0f * (3.14159265358979323846f / 180.0f), aspect,
                    nearPlane, farPlane);

    // Calculate camera position based on orbit parameters
    const float yawRad = m_cameraYawDeg * (3.14159265358979323846f / 180.0f);
    const float pitchRad =
        m_cameraPitchDeg * (3.14159265358979323846f / 180.0f);
    const float distance = std::max(0.01f, m_cameraDistance * m_cameraZoom);

    // Spherical to Cartesian conversion for orbit camera
    eyeX = m_cameraX + distance * std::cos(pitchRad) * std::sin(yawRad);
    eyeY = m_cameraY + distance * std::sin(pitchRad);
    eyeZ = m_cameraZ + distance * std::cos(pitchRad) * std::cos(yawRad);

    const float eye[3] = {eyeX, eyeY, eyeZ};
    const float center[3] = {m_cameraX, m_cameraY, m_cameraZ};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    Mat4LookAt(viewMat, eye, center, up);
  }

  float viewProj[16];
  Mat4Mul(viewProj, proj, viewMat);

  FrameUniformObject ubo{};
  std::memcpy(ubo.viewProj, viewProj, sizeof(viewProj));

  RenderDirectionalLight primaryDirectional = view.directionalLight;
  if (!primaryDirectional.enabled) {
    for (const auto &candidate : view.lights) {
      if (candidate.type == RenderLightType::Directional && candidate.enabled &&
          candidate.intensity > 0.0f) {
        primaryDirectional.enabled = true;
        primaryDirectional.direction[0] = candidate.direction[0];
        primaryDirectional.direction[1] = candidate.direction[1];
        primaryDirectional.direction[2] = candidate.direction[2];
        primaryDirectional.position[0] = candidate.position[0];
        primaryDirectional.position[1] = candidate.position[1];
        primaryDirectional.position[2] = candidate.position[2];
        primaryDirectional.color[0] = candidate.color[0];
        primaryDirectional.color[1] = candidate.color[1];
        primaryDirectional.color[2] = candidate.color[2];
        primaryDirectional.intensity = candidate.intensity;
        primaryDirectional.entityId = candidate.entityId;
        break;
      }
    }
  }

  float lightDir[3] = {primaryDirectional.direction[0],
                       primaryDirectional.direction[1],
                       primaryDirectional.direction[2]};
  if (primaryDirectional.enabled) {
    Vec3Normalize(lightDir);
  } else {
    lightDir[0] = 0.0f;
    lightDir[1] = -1.0f;
    lightDir[2] = 0.0f;
  }
  ubo.lightDir[0] = lightDir[0];
  ubo.lightDir[1] = lightDir[1];
  ubo.lightDir[2] = lightDir[2];
  ubo.lightDir[3] = 0.0f;

  const float intensity =
      primaryDirectional.enabled ? primaryDirectional.intensity : 0.0f;
  ubo.lightColor[0] = primaryDirectional.color[0] * intensity;
  ubo.lightColor[1] = primaryDirectional.color[1] * intensity;
  ubo.lightColor[2] = primaryDirectional.color[2] * intensity;
  ubo.lightColor[3] = 0.0f;

  ubo.ambientColor[0] = primaryDirectional.ambientColor[0];
  ubo.ambientColor[1] = primaryDirectional.ambientColor[1];
  ubo.ambientColor[2] = primaryDirectional.ambientColor[2];
  ubo.ambientColor[3] = 0.0f;

  std::vector<RenderLight> lights = view.lights;
  if (lights.empty() && primaryDirectional.enabled) {
    RenderLight fallback{};
    fallback.type = RenderLightType::Directional;
    fallback.enabled = true;
    fallback.entityId = primaryDirectional.entityId;
    fallback.position[0] = primaryDirectional.position[0];
    fallback.position[1] = primaryDirectional.position[1];
    fallback.position[2] = primaryDirectional.position[2];
    fallback.direction[0] = primaryDirectional.direction[0];
    fallback.direction[1] = primaryDirectional.direction[1];
    fallback.direction[2] = primaryDirectional.direction[2];
    fallback.color[0] = primaryDirectional.color[0];
    fallback.color[1] = primaryDirectional.color[1];
    fallback.color[2] = primaryDirectional.color[2];
    fallback.intensity = primaryDirectional.intensity;
    lights.push_back(fallback);
  }

  size_t directionalCount = 0;
  size_t pointCount = 0;
  size_t spotCount = 0;
  size_t totalCount = 0;

  auto pushLight = [&](const RenderLight &light) {
    if (totalCount >= kMaxLights) {
      return false;
    }
    if (!light.enabled || light.intensity <= 0.0f) {
      return false;
    }

    LightUniform &dst = ubo.lights[totalCount];
    const float range = (light.type == RenderLightType::Directional)
                            ? 0.0f
                            : std::max(0.01f, light.range);
    dst.position[0] = light.position[0];
    dst.position[1] = light.position[1];
    dst.position[2] = light.position[2];
    dst.position[3] = range;

    float dir[3] = {light.direction[0], light.direction[1], light.direction[2]};
    Vec3Normalize(dir);
    dst.direction[0] = dir[0];
    dst.direction[1] = dir[1];
    dst.direction[2] = dir[2];
    dst.direction[3] = 0.0f;

    const float scaledIntensity = light.intensity;
    dst.color[0] = light.color[0] * scaledIntensity;
    dst.color[1] = light.color[1] * scaledIntensity;
    dst.color[2] = light.color[2] * scaledIntensity;
    dst.color[3] = 0.0f;

    if (light.type == RenderLightType::Spot) {
      const float degToRad = 3.14159265358979323846f / 180.0f;
      const float innerRad = light.innerConeAngle * degToRad;
      const float outerRad = light.outerConeAngle * degToRad;
      dst.spot[0] = std::cos(innerRad);
      dst.spot[1] = std::cos(outerRad);
    } else {
      dst.spot[0] = 1.0f;
      dst.spot[1] = -1.0f;
    }
    dst.spot[2] = 0.0f;
    dst.spot[3] = 0.0f;

    ++totalCount;
    return true;
  };

  auto pushLightsOfType = [&](RenderLightType type, size_t &counter) {
    for (const auto &light : lights) {
      if (light.type != type) {
        continue;
      }
      if (pushLight(light)) {
        ++counter;
        if (totalCount >= kMaxLights) {
          break;
        }
      }
    }
  };

  pushLightsOfType(RenderLightType::Directional, directionalCount);
  pushLightsOfType(RenderLightType::Point, pointCount);
  pushLightsOfType(RenderLightType::Spot, spotCount);

  ubo.lightCounts[0] = static_cast<float>(directionalCount);
  ubo.lightCounts[1] = static_cast<float>(pointCount);
  ubo.lightCounts[2] = static_cast<float>(spotCount);
  ubo.lightCounts[3] = static_cast<float>(totalCount);

  ubo.cameraPos[0] = eyeX;
  ubo.cameraPos[1] = eyeY;
  ubo.cameraPos[2] = eyeZ;
  ubo.cameraPos[3] = 0.0f;

  const float exposure = 1.0f;
  ubo.frameParams[0] = static_cast<float>(m_debugViewMode);
  ubo.frameParams[1] = exposure;
  ubo.frameParams[2] = nearPlane;
  ubo.frameParams[3] = farPlane;

  const float metallic = 0.0f;
  const float roughness = 0.6f;
  ubo.materialParams[0] = metallic;
  ubo.materialParams[1] = roughness;
  ubo.materialParams[2] = IsSrgbFormat(m_swapchainFormat) ? 1.0f : 0.0f;
  ubo.materialParams[3] = 0.0f;

  std::memcpy(m_uniformMapped[frameIndex], &ubo, sizeof(ubo));
}

void VulkanViewport::UpdateSelectionBuffer(
    const std::vector<DrawInstance> &instances, const RenderView &view) {
  m_selectionVertexCount = 0;
  if (view.selectedEntityId == 0 || m_selectionVertexMemory == VK_NULL_HANDLE) {
    return;
  }

  const Core::EntityId selectedId = view.selectedEntityId;
  const DrawInstance *selected = nullptr;
  for (const auto &instance : instances) {
    if (instance.entityId == selectedId) {
      selected = &instance;
      break;
    }
  }

  std::array<float, 16> model{};
  bool hasModel = false;
  std::string meshId;
  if (selected) {
    std::memcpy(model.data(), selected->constants.model,
                sizeof(selected->constants.model));
    hasModel = true;
    meshId = selected->meshId;
  }

  if (meshId.empty()) {
    auto meshIt = view.meshes.find(selectedId);
    if (meshIt != view.meshes.end() && meshIt->second) {
      meshId = meshIt->second->GetMeshAssetId();
    }
  }

  if (!hasModel) {
    auto transformIt = view.transforms.find(selectedId);
    if (transformIt == view.transforms.end() ||
        transformIt->second == nullptr) {
      return;
    }

    std::unordered_map<Core::EntityId, std::array<float, 16>> worldCache;
    auto modelFor = [&](auto &&self,
                        Core::EntityId id) -> const std::array<float, 16> & {
      auto cached = worldCache.find(id);
      if (cached != worldCache.end()) {
        return cached->second;
      }

      std::array<float, 16> identity{};
      Mat4Identity(identity.data());
      auto it = view.transforms.find(id);
      if (it == view.transforms.end() || it->second == nullptr) {
        return worldCache.emplace(id, identity).first->second;
      }

      const auto *transform = it->second;
      const float radiansX =
          transform->GetRotationXDegrees() * (3.14159265358979323846f / 180.0f);
      const float radiansY =
          transform->GetRotationYDegrees() * (3.14159265358979323846f / 180.0f);
      const float radiansZ =
          transform->GetRotationZDegrees() * (3.14159265358979323846f / 180.0f);

      float t[16];
      Mat4Translation(t, transform->GetPositionX(), transform->GetPositionY(),
                      transform->GetPositionZ());

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
      Mat4Scale(s, transform->GetScaleX(), transform->GetScaleY(),
                transform->GetScaleZ());

      float tr[16];
      Mat4Mul(tr, t, r);

      float localModel[16];
      Mat4Mul(localModel, tr, s);

      if (transform->HasParent()) {
        const auto &parentModel = self(self, transform->GetParentId());
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

    const auto &world = modelFor(modelFor, selectedId);
    std::memcpy(model.data(), world.data(), sizeof(world));
    hasModel = true;
  }

  if (!hasModel) {
    return;
  }

  std::vector<Vertex> vertices;
  vertices.reserve(128); // Reserved size in CreateLineBuffers

  if (!meshId.empty() && m_assetRegistry) {
    const auto *meshData = m_assetRegistry->LoadMeshData(meshId);
    if (meshData) {
      // 1. Bounding Box
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
      for (size_t i = 0; i < corners.size(); ++i) {
        worldCorners[i] = Mat4TransformPoint(model.data(), corners[i]);
      }

      const std::array<std::pair<int, int>, 12> edges = {{
          {0, 1},
          {1, 2},
          {2, 3},
          {3, 0},
          {4, 5},
          {5, 6},
          {6, 7},
          {7, 4},
          {0, 4},
          {1, 5},
          {2, 6},
          {3, 7},
      }};

      const float normal[3] = {0.0f, 0.0f, 1.0f};
      const float boxColor[4] = {1.0f, 0.85f, 0.15f, 1.0f};
      for (const auto &edge : edges) {
        const auto &a = worldCorners[edge.first];
        const auto &b = worldCorners[edge.second];
        vertices.push_back(
            Vertex{{a[0], a[1], a[2]},
                   {normal[0], normal[1], normal[2]},
                   {boxColor[0], boxColor[1], boxColor[2], boxColor[3]},
                   {0.0f, 0.0f}});
        vertices.push_back(
            Vertex{{b[0], b[1], b[2]},
                   {normal[0], normal[1], normal[2]},
                   {boxColor[0], boxColor[1], boxColor[2], boxColor[3]},
                   {0.0f, 0.0f}});
      }
    }
  }

  // 2. Translation Gizmo (Arrows)
  const float origin[3] = {model[12], model[13], model[14]};
  const float axisLen = 2.0f;
  const float headLen = 0.4f;
  const float headWidth = 0.1f;

  auto addArrow = [&](const float dir[3], const float color[4]) {
    float end[3] = {origin[0] + dir[0] * axisLen, origin[1] + dir[1] * axisLen,
                    origin[2] + dir[2] * axisLen};

    // Shaft
    vertices.push_back(Vertex{{origin[0], origin[1], origin[2]},
                              {0, 0, 1},
                              {color[0], color[1], color[2], 1},
                              {0, 0}});
    vertices.push_back(Vertex{{end[0], end[1], end[2]},
                              {0, 0, 1},
                              {color[0], color[1], color[2], 1},
                              {0, 0}});

    // Calculate basis for arrowhead
    float up[3] = {0.0f, 1.0f, 0.0f};
    if (std::abs(dir[1]) > 0.99f) {
      up[0] = 1.0f;
      up[1] = 0.0f;
    } // Handle Y-axis case

    float right[3];
    // right = dir x up
    right[0] = dir[1] * up[2] - dir[2] * up[1];
    right[1] = dir[2] * up[0] - dir[0] * up[2];
    right[2] = dir[0] * up[1] - dir[1] * up[0];
    // normalize right
    float rLen = std::sqrt(right[0] * right[0] + right[1] * right[1] +
                           right[2] * right[2]);
    if (rLen > 0.0001f) {
      right[0] /= rLen;
      right[1] /= rLen;
      right[2] /= rLen;
    }

    float orthoUp[3];
    // orthoUp = right x dir
    orthoUp[0] = right[1] * dir[2] - right[2] * dir[1];
    orthoUp[1] = right[2] * dir[0] - right[0] * dir[2];
    orthoUp[2] = right[0] * dir[1] - right[1] * dir[0];

    float baseCenter[3] = {end[0] - dir[0] * headLen, end[1] - dir[1] * headLen,
                           end[2] - dir[2] * headLen};

    // 4 segments for cone
    for (int i = 0; i < 4; ++i) {
      float angle = i * 3.14159f / 2.0f;
      float c = std::cos(angle) * headWidth;
      float s = std::sin(angle) * headWidth;

      float p[3] = {baseCenter[0] + right[0] * c + orthoUp[0] * s,
                    baseCenter[1] + right[1] * c + orthoUp[1] * s,
                    baseCenter[2] + right[2] * c + orthoUp[2] * s};

      // Line from base point to tip
      vertices.push_back(Vertex{{p[0], p[1], p[2]},
                                {0, 0, 1},
                                {color[0], color[1], color[2], 1},
                                {0, 0}});
      vertices.push_back(Vertex{{end[0], end[1], end[2]},
                                {0, 0, 1},
                                {color[0], color[1], color[2], 1},
                                {0, 0}});
    }
  };

  const float red[4] = {0.9f, 0.1f, 0.1f, 1.0f};
  const float green[4] = {0.1f, 0.9f, 0.1f, 1.0f};
  const float blue[4] = {0.1f, 0.1f, 0.9f, 1.0f};

  const float xDir[3] = {1.0f, 0.0f, 0.0f};
  const float yDir[3] = {0.0f, 1.0f, 0.0f};
  const float zDir[3] = {0.0f, 0.0f, 1.0f};

  addArrow(xDir, red);
  addArrow(yDir, green);
  addArrow(zDir, blue);

  void *data = nullptr;
  if (!vertices.empty()) {
    vkMapMemory(m_context->GetDevice(), m_selectionVertexMemory, 0,
                sizeof(Vertex) * vertices.size(), 0, &data);
    std::memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(m_context->GetDevice(), m_selectionVertexMemory);

    m_selectionVertexCount = static_cast<uint32_t>(vertices.size());
  }
}

void VulkanViewport::UpdateLightGizmoBuffer(const RenderView &view) {
  m_lightGizmoVertexCount = 0;
  if (!view.showEditorIcons || m_lightGizmoVertexMemory == VK_NULL_HANDLE) {
    return;
  }

  std::vector<RenderLight> lights = view.lights;
  if (lights.empty() && view.directionalLight.enabled) {
    RenderLight fallback{};
    fallback.type = RenderLightType::Directional;
    fallback.enabled = true;
    fallback.entityId = view.directionalLight.entityId;
    fallback.position[0] = view.directionalLight.position[0];
    fallback.position[1] = view.directionalLight.position[1];
    fallback.position[2] = view.directionalLight.position[2];
    fallback.direction[0] = view.directionalLight.direction[0];
    fallback.direction[1] = view.directionalLight.direction[1];
    fallback.direction[2] = view.directionalLight.direction[2];
    fallback.color[0] = view.directionalLight.color[0];
    fallback.color[1] = view.directionalLight.color[1];
    fallback.color[2] = view.directionalLight.color[2];
    fallback.intensity = view.directionalLight.intensity;
    lights.push_back(fallback);
  }

  if (lights.empty()) {
    return;
  }

  std::vector<Vertex> vertices;
  vertices.reserve(std::min<size_t>(lights.size(), kMaxLights) * 96u);

  const float normal[3] = {0.0f, 1.0f, 0.0f};
  auto addLine = [&](const float a[3], const float b[3], const float color[4]) {
    vertices.push_back(Vertex{{a[0], a[1], a[2]},
                              {normal[0], normal[1], normal[2]},
                              {color[0], color[1], color[2], color[3]},
                              {0.0f, 0.0f}});
    vertices.push_back(Vertex{{b[0], b[1], b[2]},
                              {normal[0], normal[1], normal[2]},
                              {color[0], color[1], color[2], color[3]},
                              {0.0f, 0.0f}});
  };

  auto addArrow = [&](const float origin[3], const float dirIn[3], float length,
                      float headLength, float headWidth, const float color[4]) {
    float dir[3] = {dirIn[0], dirIn[1], dirIn[2]};
    Vec3Normalize(dir);
    float end[3] = {origin[0] + dir[0] * length, origin[1] + dir[1] * length,
                    origin[2] + dir[2] * length};
    addLine(origin, end, color);

    float up[3] = {0.0f, 1.0f, 0.0f};
    if (std::abs(dir[1]) > 0.99f) {
      up[0] = 1.0f;
      up[1] = 0.0f;
      up[2] = 0.0f;
    }

    float right[3];
    Vec3Cross(right, dir, up);
    Vec3Normalize(right);
    float orthoUp[3];
    Vec3Cross(orthoUp, right, dir);
    Vec3Normalize(orthoUp);

    float base[3] = {end[0] - dir[0] * headLength, end[1] - dir[1] * headLength,
                     end[2] - dir[2] * headLength};
    for (int i = 0; i < 4; ++i) {
      float angle = static_cast<float>(i) * 1.5708f;
      float c = std::cos(angle) * headWidth;
      float s = std::sin(angle) * headWidth;
      float p[3] = {base[0] + right[0] * c + orthoUp[0] * s,
                    base[1] + right[1] * c + orthoUp[1] * s,
                    base[2] + right[2] * c + orthoUp[2] * s};
      addLine(end, p, color);
    }
  };

  auto addCircle = [&](const float origin[3], const float right[3],
                       const float up[3], float radius, const float color[4]) {
    const int segments = 12;
    for (int i = 0; i < segments; ++i) {
      float a0 =
          static_cast<float>(i) * 6.28318f / static_cast<float>(segments);
      float a1 =
          static_cast<float>(i + 1) * 6.28318f / static_cast<float>(segments);
      float p0[3] = {origin[0] + std::cos(a0) * radius * right[0] +
                         std::sin(a0) * radius * up[0],
                     origin[1] + std::cos(a0) * radius * right[1] +
                         std::sin(a0) * radius * up[1],
                     origin[2] + std::cos(a0) * radius * right[2] +
                         std::sin(a0) * radius * up[2]};
      float p1[3] = {origin[0] + std::cos(a1) * radius * right[0] +
                         std::sin(a1) * radius * up[0],
                     origin[1] + std::cos(a1) * radius * right[1] +
                         std::sin(a1) * radius * up[1],
                     origin[2] + std::cos(a1) * radius * right[2] +
                         std::sin(a1) * radius * up[2]};
      addLine(p0, p1, color);
    }
  };

  size_t lightCount = 0;
  for (const auto &light : lights) {
    if (lightCount >= kMaxLights) {
      break;
    }
    ++lightCount;

    float color[4] = {std::min(1.0f, light.color[0] * 1.2f + 0.2f),
                      std::min(1.0f, light.color[1] * 1.2f + 0.2f),
                      std::min(1.0f, light.color[2] * 1.2f + 0.2f), 1.0f};
    if (!light.enabled) {
      color[0] *= 0.3f;
      color[1] *= 0.3f;
      color[2] *= 0.3f;
    }

    float pos[3] = {light.position[0], light.position[1], light.position[2]};
    float dir[3] = {light.direction[0], light.direction[1], light.direction[2]};
    Vec3Normalize(dir);

    if (light.type == RenderLightType::Directional) {
      addArrow(pos, dir, 1.5f, 0.3f, 0.15f, color);

      float up[3] = {0.0f, 1.0f, 0.0f};
      if (std::abs(dir[1]) > 0.99f) {
        up[0] = 1.0f;
        up[1] = 0.0f;
        up[2] = 0.0f;
      }
      float right[3];
      Vec3Cross(right, dir, up);
      Vec3Normalize(right);
      float orthoUp[3];
      Vec3Cross(orthoUp, right, dir);
      Vec3Normalize(orthoUp);

      addCircle(pos, right, orthoUp, 0.35f, color);
    } else if (light.type == RenderLightType::Point) {
      const float size = 0.35f;
      float a[3] = {pos[0] - size, pos[1], pos[2]};
      float b[3] = {pos[0] + size, pos[1], pos[2]};
      float c[3] = {pos[0], pos[1] - size, pos[2]};
      float d[3] = {pos[0], pos[1] + size, pos[2]};
      float e[3] = {pos[0], pos[1], pos[2] - size};
      float f[3] = {pos[0], pos[1], pos[2] + size};
      addLine(a, b, color);
      addLine(c, d, color);
      addLine(e, f, color);
    } else {
      const float range = std::max(0.1f, light.range);
      addArrow(pos, dir, range * 0.6f, range * 0.12f, range * 0.08f, color);

      float up[3] = {0.0f, 1.0f, 0.0f};
      if (std::abs(dir[1]) > 0.99f) {
        up[0] = 1.0f;
        up[1] = 0.0f;
        up[2] = 0.0f;
      }
      float right[3];
      Vec3Cross(right, dir, up);
      Vec3Normalize(right);
      float orthoUp[3];
      Vec3Cross(orthoUp, right, dir);
      Vec3Normalize(orthoUp);

      const float outerRad =
          light.outerConeAngle * (3.14159265358979323846f / 180.0f);
      const float coneRadius = std::tan(outerRad) * range;
      float end[3] = {pos[0] + dir[0] * range, pos[1] + dir[1] * range,
                      pos[2] + dir[2] * range};
      for (int i = 0; i < 4; ++i) {
        float angle = static_cast<float>(i) * 1.5708f;
        float offset[3] = {right[0] * std::cos(angle) * coneRadius +
                               orthoUp[0] * std::sin(angle) * coneRadius,
                           right[1] * std::cos(angle) * coneRadius +
                               orthoUp[1] * std::sin(angle) * coneRadius,
                           right[2] * std::cos(angle) * coneRadius +
                               orthoUp[2] * std::sin(angle) * coneRadius};
        float rim[3] = {end[0] + offset[0], end[1] + offset[1],
                        end[2] + offset[2]};
        addLine(pos, rim, color);
      }
    }
  }

  if (vertices.empty()) {
    return;
  }

  void *data = nullptr;
  vkMapMemory(m_context->GetDevice(), m_lightGizmoVertexMemory, 0,
              sizeof(Vertex) * vertices.size(), 0, &data);
  std::memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
  vkUnmapMemory(m_context->GetDevice(), m_lightGizmoVertexMemory);

  m_lightGizmoVertexCount = static_cast<uint32_t>(vertices.size());
}

void VulkanViewport::SetCameraPosition(float x, float y, float z) noexcept {
  m_cameraX = x;
  m_cameraY = y;
  m_cameraZ = z;
}

void VulkanViewport::SetCameraRotation(float yawDeg, float pitchDeg) noexcept {
  m_cameraYawDeg = yawDeg;
  m_cameraPitchDeg = pitchDeg;
}

void VulkanViewport::SetCameraZoom(float zoom) noexcept { m_cameraZoom = zoom; }

void VulkanViewport::SetCameraDistance(float distance) noexcept {
  m_cameraDistance = distance;
}

void VulkanViewport::ResetCamera() noexcept {
  m_cameraX = 0.0f;
  m_cameraY = 0.0f;
  m_cameraZ = 0.0f;
  m_cameraYawDeg = 30.0f;
  m_cameraPitchDeg = 25.0f;
  m_cameraZoom = 1.0f;
  m_cameraDistance = 5.0f;
}

void VulkanViewport::FocusOnBounds(float centerX, float centerY, float centerZ,
                                   float radius, float padding) noexcept {
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
void VulkanViewport::UpdateMetalLayerSize(int width, int height) {
  if (!m_metalLayer || !m_nativeHandle) {
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

  // Retrieve the window from the view to get the correct backing scale factor
  // (DPI). If the window is not yet available, fall back to the layer's current
  // scale.
  double scale = 1.0;
  id window = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(view, selWindow);
  if (window) {
    scale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(
        window, selBackingScaleFactor);
  } else {
    scale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(
        layer, selContentsScale);
  }

  if (scale <= 0.0) {
    scale = 1.0;
  }

  // Ensure the layer matches the window's scale factor.
  reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(
      layer, selSetContentsScale, scale);

  CGSize drawableSize;
  drawableSize.width = static_cast<double>(width) * scale;
  drawableSize.height = static_cast<double>(height) * scale;

  reinterpret_cast<void (*)(id, SEL, CGSize)>(objc_msgSend)(
      layer, selSetDrawableSize, drawableSize);

  // Keep layer frame/bounds in sync with view size (logical points).
  CGRect bounds = CGRectMake(0, 0, static_cast<CGFloat>(width),
                             static_cast<CGFloat>(height));
  reinterpret_cast<void (*)(id, SEL, CGRect)>(objc_msgSend)(layer, selSetBounds,
                                                            bounds);
  reinterpret_cast<void (*)(id, SEL, CGRect)>(objc_msgSend)(layer, selSetFrame,
                                                            bounds);
}
#endif

std::vector<VulkanViewport::DrawInstance>
VulkanViewport::InstancesFromView(const RenderView &view,
                                  float timeSeconds) const {
  std::vector<DrawInstance> instances;
  instances.reserve(view.instances.size());

  // Optimization: Avoid copying maps if possible by using pointers
  const std::unordered_map<Core::EntityId, const Scene::TransformComponent *>
      *transformLookupPtr = &view.transforms;
  std::unordered_map<Core::EntityId, const Scene::TransformComponent *>
      localTransformLookup;

  if (view.transforms.empty()) {
    // View didn't provide pre-built map, so build it locally
    for (const auto &instance : view.instances) {
      if (instance.transform) {
        localTransformLookup.emplace(instance.entityId, instance.transform);
      }
    }
    transformLookupPtr = &localTransformLookup;
  }
  const auto &transformLookup = *transformLookupPtr;

  const std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent *>
      *meshLookupPtr = &view.meshes;
  std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent *>
      localMeshLookup;

  if (view.meshes.empty()) {
    for (const auto &instance : view.instances) {
      if (instance.mesh) {
        localMeshLookup.emplace(instance.entityId, instance.mesh);
      }
    }
    meshLookupPtr = &localMeshLookup;
  }
  const auto &meshLookup = *meshLookupPtr;

  std::unordered_map<Core::EntityId, std::array<float, 16>> worldCache;
  auto modelFor = [&](auto &&self,
                      Core::EntityId id) -> const std::array<float, 16> & {
    auto cached = worldCache.find(id);
    if (cached != worldCache.end()) {
      return cached->second;
    }

    std::array<float, 16> identity{};
    Core::Math::Mat4Identity(identity.data());

    auto it = transformLookup.find(id);
    if (it == transformLookup.end() || it->second == nullptr) {
      return worldCache.emplace(id, identity).first->second;
    }

    const auto *transform = it->second;
    const auto meshIt = meshLookup.find(id);
    const float spinDeg =
        (meshIt != meshLookup.end() && meshIt->second)
            ? meshIt->second->GetRotationSpeedDegPerSec() * timeSeconds
            : 0.0f;

    static constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float radiansX = transform->GetRotationXDegrees() * kDegToRad;
    const float radiansY = transform->GetRotationYDegrees() * kDegToRad;
    const float radiansZ =
        (transform->GetRotationZDegrees() + spinDeg) * kDegToRad;

    float localModel[16];
    Core::Math::Mat4Compose(
        localModel, transform->GetPositionX(), transform->GetPositionY(),
        transform->GetPositionZ(), radiansX, radiansY, radiansZ,
        transform->GetScaleX(), transform->GetScaleY(), transform->GetScaleZ());

    if (transform->HasParent()) {
      const auto &parentModel = self(self, transform->GetParentId());
      float world[16];
      Core::Math::Mat4Mul(world, parentModel.data(), localModel);
      std::array<float, 16> stored{};
      std::memcpy(stored.data(), world, sizeof(world));
      return worldCache.emplace(id, stored).first->second;
    }

    std::array<float, 16> stored{};
    std::memcpy(stored.data(), localModel, sizeof(localModel));
    return worldCache.emplace(id, stored).first->second;
  };

  auto appendInstances = [&](const std::vector<RenderInstance> &source) {
    for (const auto &instance : source) {
      const Scene::TransformComponent *transform = instance.transform;
      if (!transform) {
        auto it = transformLookup.find(instance.entityId);
        if (it != transformLookup.end()) {
          transform = it->second;
        }
      }

      const Scene::MeshRendererComponent *mesh = instance.mesh;
      if (!mesh) {
        auto it = meshLookup.find(instance.entityId);
        if (it != meshLookup.end()) {
          mesh = it->second;
        }
      }

      if (!transform && !instance.hasModel) {
        continue;
      }

      DrawInstance draw{};
      draw.entityId = instance.entityId;
      draw.constants.entityId = static_cast<uint32_t>(instance.entityId);
      draw.constants.flags = 0;

      if (instance.hasModel) {
        std::memcpy(draw.constants.model, instance.model,
                    sizeof(draw.constants.model));
      } else {
        const auto &model = modelFor(modelFor, instance.entityId);
        std::memcpy(draw.constants.model, model.data(),
                    sizeof(draw.constants.model));
      }

      if (mesh) {
        const auto color = mesh->GetColor();
        draw.constants.color[0] = color[0];
        draw.constants.color[1] = color[1];
        draw.constants.color[2] = color[2];
        draw.constants.color[3] = 1.0f;
      } else {
        draw.constants.color[0] = 1.0f;
        draw.constants.color[1] = 1.0f;
        draw.constants.color[2] = 1.0f;
        draw.constants.color[3] = 1.0f;
      }

      draw.meshId = instance.meshAssetId;
      if (draw.meshId.empty() && mesh) {
        draw.meshId = mesh->GetMeshAssetId();
      }
      draw.textureId = instance.albedoTextureId;
      if (draw.textureId.empty() && mesh) {
        draw.textureId = mesh->GetAlbedoTextureId();
      }

      instances.push_back(std::move(draw));
    }
  };

  if (!view.batches.empty()) {
    for (const auto &batch : view.batches) {
      appendInstances(batch.instances);
    }
  } else {
    appendInstances(view.instances);
  }

  if (view.showEditorIcons) {
    float cameraPos[3] = {0.0f, 0.0f, 0.0f};
    float cameraForward[3] = {0.0f, 0.0f, -1.0f};
    float cameraUp[3] = {0.0f, 1.0f, 0.0f};
    if (view.camera.enabled) {
      cameraPos[0] = view.camera.position[0];
      cameraPos[1] = view.camera.position[1];
      cameraPos[2] = view.camera.position[2];
      cameraForward[0] = view.camera.forward[0];
      cameraForward[1] = view.camera.forward[1];
      cameraForward[2] = view.camera.forward[2];
      Vec3Normalize(cameraForward);
      cameraUp[0] = view.camera.up[0];
      cameraUp[1] = view.camera.up[1];
      cameraUp[2] = view.camera.up[2];
      Vec3Normalize(cameraUp);
    } else {
      const float yawRad = m_cameraYawDeg * (3.14159265358979323846f / 180.0f);
      const float pitchRad =
          m_cameraPitchDeg * (3.14159265358979323846f / 180.0f);
      const float distance = std::max(0.01f, m_cameraDistance * m_cameraZoom);
      const float eyeX =
          m_cameraX + distance * std::cos(pitchRad) * std::sin(yawRad);
      const float eyeY = m_cameraY + distance * std::sin(pitchRad);
      const float eyeZ =
          m_cameraZ + distance * std::cos(pitchRad) * std::cos(yawRad);
      cameraPos[0] = eyeX;
      cameraPos[1] = eyeY;
      cameraPos[2] = eyeZ;
      const float center[3] = {m_cameraX, m_cameraY, m_cameraZ};
      cameraForward[0] = center[0] - eyeX;
      cameraForward[1] = center[1] - eyeY;
      cameraForward[2] = center[2] - eyeZ;
      Vec3Normalize(cameraForward);
    }

    float cameraRight[3];
    Vec3Cross(cameraRight, cameraForward, cameraUp);
    Vec3Normalize(cameraRight);
    Vec3Cross(cameraUp, cameraRight, cameraForward);
    Vec3Normalize(cameraUp);

    auto appendIcon = [&](Core::EntityId id, const float pos[3],
                          const float color[4], float scaleFactor) {
      float dx = pos[0] - cameraPos[0];
      float dy = pos[1] - cameraPos[1];
      float dz = pos[2] - cameraPos[2];
      float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
      float scale = std::clamp(dist * scaleFactor, 0.15f, 1.5f);

      float forward[3] = {-cameraForward[0], -cameraForward[1],
                          -cameraForward[2]};
      float model[16];
      Mat4Identity(model);
      model[0] = cameraRight[0] * scale;
      model[1] = cameraRight[1] * scale;
      model[2] = cameraRight[2] * scale;
      model[4] = cameraUp[0] * scale;
      model[5] = cameraUp[1] * scale;
      model[6] = cameraUp[2] * scale;
      model[8] = forward[0] * scale;
      model[9] = forward[1] * scale;
      model[10] = forward[2] * scale;
      model[12] = pos[0];
      model[13] = pos[1];
      model[14] = pos[2];

      DrawInstance draw{};
      draw.entityId = id;
      draw.constants.entityId = static_cast<uint32_t>(id);
      draw.constants.flags = kInstanceFlagUnlit;
      draw.constants.color[0] = color[0];
      draw.constants.color[1] = color[1];
      draw.constants.color[2] = color[2];
      draw.constants.color[3] = color[3];
      std::memcpy(draw.constants.model, model, sizeof(model));
      draw.meshId = kIconMeshId;
      instances.push_back(std::move(draw));
    };

    for (const auto &cam : view.cameras) {
      const float camColor[4] = {0.2f, 0.75f, 1.0f, 1.0f};
      appendIcon(cam.entityId, cam.position, camColor, 0.03f);
    }

    for (const auto &light : view.lights) {
      float color[4] = {0.9f, 0.8f, 0.35f, 1.0f};
      switch (light.type) {
      case RenderLightType::Point:
        color[0] = 1.0f;
        color[1] = 0.65f;
        color[2] = 0.25f;
        break;
      case RenderLightType::Spot:
        color[0] = 1.0f;
        color[1] = 0.55f;
        color[2] = 0.20f;
        break;
      default:
        break;
      }
      color[0] = std::min(1.0f, color[0] * light.color[0] + 0.15f);
      color[1] = std::min(1.0f, color[1] * light.color[1] + 0.15f);
      color[2] = std::min(1.0f, color[2] * light.color[2] + 0.15f);
      if (!light.enabled) {
        color[0] *= 0.35f;
        color[1] *= 0.35f;
        color[2] *= 0.35f;
      }
      float pos[3] = {light.position[0], light.position[1], light.position[2]};
      appendIcon(light.entityId, pos, color, 0.028f);
    }
  }

  return instances;
}

const VulkanViewport::GpuMesh *
VulkanViewport::ResolveMesh(const std::string &assetId) {
  if (assetId.empty() || !m_context || !m_context->IsInitialized()) {
    return nullptr;
  }
  if (assetId == kIconMeshId) {
    return (m_iconMesh.vertexBuffer != VK_NULL_HANDLE) ? &m_iconMesh : nullptr;
  }

  auto cached = m_meshCache.find(assetId);
  if (cached != m_meshCache.end()) {
    return &cached->second;
  }

  if (!m_assetRegistry) {
    return nullptr;
  }

  const auto *meshData = m_assetRegistry->LoadMeshData(assetId);
  if (!meshData || meshData->positions.empty()) {
    if (m_missingMeshes.emplace(assetId).second && m_context) {
      m_context->Log(
          LogSeverity::Warning,
          "VulkanViewport: mesh data missing or unsupported for asset '" +
              assetId + "'");
    }
    return nullptr;
  }
  m_missingMeshes.erase(assetId);

  // glTF indices are optional. If the mesh is non-indexed, generate sequential
  // indices.
  std::vector<uint32_t> generatedIndices;
  const std::vector<uint32_t> *indexSource = &meshData->indices;
  if (indexSource->empty()) {
    generatedIndices.resize(meshData->positions.size());
    for (size_t i = 0; i < generatedIndices.size(); ++i) {
      generatedIndices[i] = static_cast<uint32_t>(i);
    }
    indexSource = &generatedIndices;
  }

  std::vector<Vertex> vertices;
  vertices.reserve(meshData->positions.size());
  for (size_t i = 0; i < meshData->positions.size(); ++i) {
    const auto &pos = meshData->positions[i];
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    if (i < meshData->colors.size()) {
      r = meshData->colors[i][0];
      g = meshData->colors[i][1];
      b = meshData->colors[i][2];
      a = meshData->colors[i][3];
    }

    float nx = 0.0f, ny = 0.0f, nz = 1.0f;
    if (i < meshData->normals.size()) {
      nx = meshData->normals[i][0];
      ny = meshData->normals[i][1];
      nz = meshData->normals[i][2];
    }

    float u = 0.0f, v = 0.0f;
    if (i < meshData->uvs.size()) {
      u = meshData->uvs[i][0];
      v = meshData->uvs[i][1];
    }

    vertices.push_back(
        Vertex{{pos[0], pos[1], pos[2]}, {nx, ny, nz}, {r, g, b, a}, {u, v}});
  }

  VkDevice device = m_context->GetDevice();
  VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

  GpuMesh mesh{};
  VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingVertexMemory = VK_NULL_HANDLE;
  VkBuffer stagingIndexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingIndexMemory = VK_NULL_HANDLE;
  try {
    const VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    const VkDeviceSize indexSize =
        sizeof(std::uint32_t) * static_cast<VkDeviceSize>(indexSource->size());

    CreateBuffer(gpu, device, vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingVertexBuffer, stagingVertexMemory);

    void *vData = nullptr;
    vkMapMemory(device, stagingVertexMemory, 0, vertexSize, 0, &vData);
    std::memcpy(vData, vertices.data(), static_cast<size_t>(vertexSize));
    vkUnmapMemory(device, stagingVertexMemory);

    CreateBuffer(gpu, device, vertexSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh.vertexBuffer,
                 mesh.vertexMemory);
    CopyBuffer(stagingVertexBuffer, mesh.vertexBuffer, vertexSize);

    vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
    vkFreeMemory(device, stagingVertexMemory, nullptr);
    stagingVertexBuffer = VK_NULL_HANDLE;
    stagingVertexMemory = VK_NULL_HANDLE;

    CreateBuffer(gpu, device, indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingIndexBuffer, stagingIndexMemory);

    void *iData = nullptr;
    vkMapMemory(device, stagingIndexMemory, 0, indexSize, 0, &iData);
    std::memcpy(iData, indexSource->data(), static_cast<size_t>(indexSize));
    vkUnmapMemory(device, stagingIndexMemory);

    CreateBuffer(gpu, device, indexSize,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh.indexBuffer,
                 mesh.indexMemory);
    CopyBuffer(stagingIndexBuffer, mesh.indexBuffer, indexSize);

    vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
    vkFreeMemory(device, stagingIndexMemory, nullptr);
    stagingIndexBuffer = VK_NULL_HANDLE;
    stagingIndexMemory = VK_NULL_HANDLE;

    mesh.indexCount = static_cast<uint32_t>(indexSource->size());
  } catch (const std::exception &ex) {
    if (stagingVertexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
    }
    if (stagingVertexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, stagingVertexMemory, nullptr);
    }
    if (stagingIndexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
    }
    if (stagingIndexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, stagingIndexMemory, nullptr);
    }
    if (mesh.vertexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
    }
    if (mesh.vertexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, mesh.vertexMemory, nullptr);
    }
    if (mesh.indexBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
    }
    if (mesh.indexMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device, mesh.indexMemory, nullptr);
    }

    if (m_context) {
      m_context->Log(LogSeverity::Error,
                     std::string("Mesh upload failed: ") + ex.what());
    }
    return nullptr;
  }

  auto [it, inserted] = m_meshCache.emplace(assetId, std::move(mesh));
  if (!inserted) {
    it->second = std::move(mesh);
  }
  return &it->second;
}

void VulkanViewport::TransitionImageLayout(VkImage image, VkFormat format,
                                           VkImageLayout oldLayout,
                                           VkImageLayout newLayout) {
  if (m_commandPool == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "VulkanViewport: command pool missing for texture upload");
  }

  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = m_commandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, &cmd) !=
      VK_SUCCESS) {
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
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(format)) {
      aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  }
  barrier.subresourceRange.aspectMask = aspectMask;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    vkEndCommandBuffer(cmd);
    vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);
    throw std::runtime_error("Unsupported texture layout transition");
  }

  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_context->GetGraphicsQueue());

  vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);
}

void VulkanViewport::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                                VkDeviceSize size) {
  if (m_commandPool == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "VulkanViewport: command pool missing for buffer copy");
  }

  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = m_commandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, &cmd) !=
      VK_SUCCESS) {
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

void VulkanViewport::CopyBufferToImage(VkBuffer buffer, VkImage image,
                                       uint32_t width, uint32_t height) {
  if (m_commandPool == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "VulkanViewport: command pool missing for texture copy");
  }

  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = m_commandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(m_context->GetDevice(), &alloc, &cmd) !=
      VK_SUCCESS) {
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
  vkCmdCopyBufferToImage(cmd, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_context->GetGraphicsQueue());

  vkFreeCommandBuffers(m_context->GetDevice(), m_commandPool, 1, &cmd);
}

VulkanViewport::GpuTexture
VulkanViewport::CreateTextureFromPixels(const unsigned char *pixels,
                                        uint32_t width, uint32_t height) {
  VkDevice device = m_context->GetDevice();
  VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

  const VkDeviceSize imageSize =
      static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;

  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
  CreateBuffer(gpu, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingMemory);

  void *data = nullptr;
  vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
  std::memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device, stagingMemory);

  GpuTexture texture{};
  texture.width = width;
  texture.height = height;

  const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
  CreateImage(gpu, device, width, height, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image,
              texture.memory);

  TransitionImageLayout(texture.image, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CopyBufferToImage(stagingBuffer, texture.image, width, height);
  TransitionImageLayout(texture.image, format,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingMemory, nullptr);

  texture.view =
      CreateImageView(device, texture.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
  texture.sampler = m_textureSampler;

  if (m_textureDescriptorPools.empty() ||
      m_textureDescriptorSetLayout == VK_NULL_HANDLE) {
    throw std::runtime_error("Texture descriptor pool/layout not available");
  }

  VkDescriptorSetAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc.descriptorSetCount = 1;
  alloc.pSetLayouts = &m_textureDescriptorSetLayout;

  VkDescriptorPool pool =
      m_textureDescriptorPools[m_activeTextureDescriptorPool];
  alloc.descriptorPool = pool;
  VkResult allocRes =
      vkAllocateDescriptorSets(device, &alloc, &texture.descriptorSet);
  if (allocRes == VK_ERROR_OUT_OF_POOL_MEMORY ||
      allocRes == VK_ERROR_FRAGMENTED_POOL) {
    m_textureDescriptorPools.push_back(CreateTextureDescriptorPoolInternal());
    m_activeTextureDescriptorPool = m_textureDescriptorPools.size() - 1;
    pool = m_textureDescriptorPools[m_activeTextureDescriptorPool];
    alloc.descriptorPool = pool;
    allocRes = vkAllocateDescriptorSets(device, &alloc, &texture.descriptorSet);
  }
  if (allocRes != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate texture descriptor set");
  }
  texture.descriptorPool = pool;

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

const VulkanViewport::GpuTexture *
VulkanViewport::ResolveTexture(const std::string &assetId) {
  if (!m_context || !m_context->IsInitialized()) {
    return nullptr;
  }

  if (assetId.empty()) {
    return &m_defaultTexture;
  }

  auto cached = m_textureCache.find(assetId);
  if (cached != m_textureCache.end()) {
    return &cached->second;
  }

  if (!m_assetRegistry) {
    return &m_defaultTexture;
  }

  std::filesystem::path sourcePath;
  if (const auto *entry = m_assetRegistry->FindEntry(assetId)) {
    sourcePath = entry->path;
  } else {
    sourcePath = std::filesystem::path(assetId);
    if (!sourcePath.is_absolute()) {
      const auto root = m_assetRegistry->GetRootPath();
      if (!root.empty()) {
        sourcePath = root / sourcePath;
      }
    }
  }

  std::error_code ec;
  if (sourcePath.empty() || !std::filesystem::exists(sourcePath, ec)) {
    if (m_missingTextures.emplace(assetId).second && m_context) {
      m_context->Log(LogSeverity::Warning,
                     "VulkanViewport: texture asset not found '" + assetId +
                         "'");
    }
    return &m_defaultTexture;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc *pixels = stbi_load(sourcePath.string().c_str(), &width, &height,
                              &channels, STBI_rgb_alpha);
  if (!pixels || width <= 0 || height <= 0) {
    if (m_missingTextures.emplace(assetId).second && m_context) {
      m_context->Log(LogSeverity::Warning,
                     "VulkanViewport: failed to load texture '" + assetId +
                         "'");
    }
    if (pixels) {
      stbi_image_free(pixels);
    }
    return &m_defaultTexture;
  }

  GpuTexture texture{};
  try {
    texture = CreateTextureFromPixels(pixels, static_cast<uint32_t>(width),
                                      static_cast<uint32_t>(height));
  } catch (const std::exception &ex) {
    if (m_context) {
      m_context->Log(LogSeverity::Error,
                     std::string("Texture upload failed: ") + ex.what());
    }
    stbi_image_free(pixels);
    return &m_defaultTexture;
  }
  stbi_image_free(pixels);

  auto [it, inserted] = m_textureCache.emplace(assetId, std::move(texture));
  if (!inserted) {
    it->second = std::move(texture);
  }
  return &it->second;
}

void VulkanViewport::CreateSyncObjects() {
  m_imageAvailable.resize(kMaxFramesInFlight);
  m_inFlight.resize(kMaxFramesInFlight);

  VkSemaphoreCreateInfo sem{};
  sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (vkCreateSemaphore(m_context->GetDevice(), &sem, nullptr,
                          &m_imageAvailable[i]) != VK_SUCCESS ||
        vkCreateFence(m_context->GetDevice(), &fence, nullptr,
                      &m_inFlight[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create sync objects");
    }
  }
}

void VulkanViewport::CreateQueryPools() {
  if (!m_context || !m_context->IsInitialized()) {
    return;
  }

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(m_context->GetPhysicalDevice(), &props);
  m_timestampPeriod = props.limits.timestampPeriod;
  m_timestampsSupported = props.limits.timestampComputeAndGraphics == VK_TRUE;

  VkDevice device = m_context->GetDevice();
  for (auto &pool : m_queryPools) {
    if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
      vkDestroyQueryPool(device, pool, nullptr);
    }
    pool = VK_NULL_HANDLE;
  }

  if (!m_timestampsSupported) {
    return;
  }

  VkQueryPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = kPassCount * 2;

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (vkCreateQueryPool(device, &info, nullptr, &m_queryPools[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create timestamp query pool");
    }
  }
}

std::string VulkanViewport::ShaderPath(const char *filename) const {
  namespace fs = std::filesystem;

  const fs::path candidates[] = {
      fs::path("shaders") / filename,
      fs::path("build") / "shaders" / filename,
      fs::path("..") / "shaders" / filename,
  };

  for (const auto &candidate : candidates) {
    std::error_code ec;
    if (fs::exists(candidate, ec)) {
      return candidate.string();
    }
  }

  // Fall back to the original relative path for error reporting.
  return (fs::path("shaders") / filename).string();
}

std::vector<char>
VulkanViewport::ReadFileBinary(const std::string &path) const {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open shader file: " + path);
  }

  const std::streamsize size = file.tellg();
  std::vector<char> buffer(static_cast<size_t>(size));
  file.seekg(0);
  file.read(buffer.data(), size);
  return buffer;
}

VkShaderModule
VulkanViewport::CreateShaderModule(const std::vector<char> &code) const {
  VkShaderModuleCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create.codeSize = code.size();
  create.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(m_context->GetDevice(), &create, nullptr, &module) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create shader module");
  }
  return module;
}
} // namespace Aetherion::Rendering
