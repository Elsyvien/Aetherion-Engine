#include "Aetherion/Rendering/VulkanViewport.h"

#include "Aetherion/Rendering/VulkanContext.h"

#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

namespace Aetherion::Rendering
{
namespace
{
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

    if (m_ready)
    {
        return;
    }

    CreateSurface(nativeHandle);
    CreateSwapchain(width, height);
    CreateRenderPass();
    CreatePipeline();
    CreateFramebuffers();
    CreateCommandPoolAndBuffers();
    CreateSyncObjects();

    m_ready = true;
}

void VulkanViewport::Shutdown()
{
    if (!m_context || !m_context->IsInitialized())
    {
        return;
    }

    VkDevice device = m_context->GetDevice();

    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
    }

    for (auto fence : m_inFlight)
    {
        if (fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device, fence, nullptr);
        }
    }
    m_inFlight.clear();

    for (auto sem : m_renderFinishedPerImage)
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_renderFinishedPerImage.clear();

    for (auto sem : m_imageAvailable)
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_imageAvailable.clear();

    m_imagesInFlight.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    DestroySwapchain();

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    m_ready = false;
}

void VulkanViewport::Resize(int width, int height)
{
    if (!m_ready)
    {
        return;
    }

    if (width <= 0 || height <= 0)
    {
        return;
    }

    VkDevice device = m_context->GetDevice();
    vkDeviceWaitIdle(device);

    DestroySwapchain();
    CreateSwapchain(width, height);
    CreateFramebuffers();

    // Swapchain image count may have changed; rebuild per-image sync.
    for (auto sem : m_renderFinishedPerImage)
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_renderFinishedPerImage.clear();
    m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_renderFinishedPerImage.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        if (vkCreateSemaphore(device, &sem, nullptr, &m_renderFinishedPerImage[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to recreate per-image renderFinished semaphore");
        }
    }

    // Command buffers need to be re-recorded because framebuffers changed.
}

void VulkanViewport::RenderFrame()
{
    if (!m_ready)
    {
        return;
    }

    VkDevice device = m_context->GetDevice();
    VkQueue graphicsQueue = m_context->GetGraphicsQueue();

    VkFence inFlight = m_inFlight[m_frameIndex];
    vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlight);

    uint32_t imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX, m_imageAvailable[m_frameIndex], VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    // If this swapchain image is already being used by a previous frame, wait for it.
    if (imageIndex < m_imagesInFlight.size() && m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    if (imageIndex < m_imagesInFlight.size())
    {
        m_imagesInFlight[imageIndex] = inFlight;
    }

    vkResetCommandBuffer(m_commandBuffers[m_frameIndex], 0);
    RecordCommandBuffer(imageIndex);

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

    if (vkQueueSubmit(graphicsQueue, 1, &submit, inFlight) != VK_SUCCESS)
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

    VkResult pres = vkQueuePresentKHR(graphicsQueue, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
    {
        // Will be handled on next resize.
    }
    else if (pres != VK_SUCCESS)
    {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    m_frameIndex = (m_frameIndex + 1) % kMaxFramesInFlight;
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
#else
    (void)nativeHandle;
    throw std::runtime_error("VulkanViewport: platform not supported in this build");
#endif

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_context->GetPhysicalDevice(), m_context->GetGraphicsQueueFamilyIndex(), m_surface, &presentSupport);
    if (!presentSupport)
    {
        throw std::runtime_error("VulkanViewport: graphics queue family does not support present (needs separate present queue family)");
    }
}

void VulkanViewport::CreateSwapchain(int width, int height)
{
    VkPhysicalDevice gpu = m_context->GetPhysicalDevice();

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, m_surface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_surface, &formatCount, formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(presentModeCount);
    if (presentModeCount)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface, &presentModeCount, modes.data());
    }

    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(modes);
    VkExtent2D extent = ChooseExtent(caps, width, height);

    uint32_t imageCount = caps.minImageCount + 1;
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
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = caps.currentTransform;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = presentMode;
    create.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_context->GetDevice(), &create, nullptr, &m_swapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swapchain");
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
        if (sem != VK_NULL_HANDLE)
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
    VkDevice device = m_context->GetDevice();

    for (auto fb : m_framebuffers)
    {
        if (fb != VK_NULL_HANDLE)
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

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
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

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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

    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

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

void VulkanViewport::RecordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBuffer cb = m_commandBuffers[m_frameIndex];

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS)
    {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    VkClearValue clear{};
    // Very visible debug clear color.
    clear.color = {{0.9f, 0.0f, 0.8f, 1.0f}};

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

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdDraw(cb, 3, 1, 0, 0);

    vkCmdEndRenderPass(cb);

    if (vkEndCommandBuffer(cb) != VK_SUCCESS)
    {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
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
    // Exe is run from build directory; shaders are generated into build-dir/shaders.
    return std::string("shaders/") + filename;
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
