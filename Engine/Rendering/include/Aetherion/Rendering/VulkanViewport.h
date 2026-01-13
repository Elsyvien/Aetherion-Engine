#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <vulkan/vulkan.h>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Rendering/RenderView.h"

namespace Aetherion::Rendering {
class VulkanContext;
}

namespace Aetherion::Rendering {
class VulkanViewport {
public:
  static constexpr uint32_t kPassCount = 4;
  enum class DebugViewMode : uint32_t {
    Final = 0,
    Normals = 1,
    Roughness = 2,
    Metallic = 3,
    Albedo = 4,
    Depth = 5,
    EntityId = 6,
  };

  struct PassStats {
    const char *name = "";
    double cpuMs = 0.0;
    double gpuMs = 0.0;
  };

  struct FrameStats {
    double cpuTotalMs = 0.0;
    double gpuTotalMs = 0.0;
    std::array<PassStats, kPassCount> passes{};
    bool valid = false;
  };

  struct PickResult {
    Core::EntityId entityId{0};
    uint32_t x{0};
    uint32_t y{0};
    bool valid{false};
  };

  explicit VulkanViewport(
      std::shared_ptr<VulkanContext> context,
      std::shared_ptr<Assets::AssetRegistry> assetRegistry = nullptr);
  ~VulkanViewport();

  VulkanViewport(const VulkanViewport &) = delete;
  VulkanViewport &operator=(const VulkanViewport &) = delete;

  void Initialize(void *nativeHandle, int width, int height);
  void Resize(int width, int height);
  void RenderFrame(float deltaTimeSeconds, const RenderView &view);
  void Shutdown();
  void HandleAssetChanges(
      const std::vector<Assets::AssetRegistry::AssetChange> &changes);

  [[nodiscard]] bool IsReady() const noexcept { return m_ready; }
  void SetLoggingEnabled(bool enabled) noexcept { m_verboseLogging = enabled; }
  void SetVsyncEnabled(bool enabled) noexcept;
  [[nodiscard]] bool IsVsyncEnabled() const noexcept { return m_vsyncEnabled; }

  void SetDebugViewMode(DebugViewMode mode) noexcept { m_debugViewMode = mode; }
  [[nodiscard]] DebugViewMode GetDebugViewMode() const noexcept {
    return m_debugViewMode;
  }

  void RequestPick(uint32_t x, uint32_t y) noexcept;
  void SetPickFlipY(bool enabled) noexcept { m_pickFlipY = enabled; }
  [[nodiscard]] PickResult GetLastPickResult() const noexcept {
    return m_lastPickResult;
  }
  void ClearPickResult() noexcept { m_lastPickResult.valid = false; }

  [[nodiscard]] FrameStats GetLastFrameStats() const noexcept {
    return m_lastFrameStats;
  }

  // Camera control
  void SetCameraPosition(float x, float y, float z) noexcept;
  void SetCameraRotation(float yawDeg, float pitchDeg) noexcept;
  void SetCameraZoom(float zoom) noexcept;
  void SetCameraDistance(float distance) noexcept;
  void ResetCamera() noexcept;
  void FocusOnBounds(float centerX, float centerY, float centerZ, float radius,
                     float padding = 1.25f) noexcept;

private:
  static constexpr uint32_t kMaxFramesInFlight = 2;
  static constexpr uint32_t kMaxTextureDescriptors = 128;
  static constexpr uint32_t kMaxLights = 8;
  static constexpr uint32_t kShadowCascadeCount = 4;
  static constexpr uint32_t kShadowMapResolution = 2048;
  struct InstancePushConstants {
    float model[16]{};
    float color[4]{};
    uint32_t entityId{0};
    uint32_t flags{0};
    float padding[2]{};
  };

  struct PickRequest {
    bool pending{false};
    uint32_t x{0};
    uint32_t y{0};
  };

  struct PickReadback {
    bool inFlight{false};
    uint32_t x{0};
    uint32_t y{0};
  };

  struct DeferredDeletion {
    uint32_t framesRemaining{0};
    std::function<void()> callback;
  };

  struct DrawInstance {
    InstancePushConstants constants{};
    Core::EntityId entityId{0};
    std::string meshId;
    std::string materialId;
    std::string textureId; // Deprecated, kept for immediate compile fix
  };

  struct InstanceData {
    float model[16]{};
    float color[4]{};
    uint32_t ids[4]{};
    float bounds[4]{};
  };

  struct DrawBatch {
    std::string meshId;
    std::string materialId;
    uint32_t inputOffset{0};
    uint32_t inputCount{0};
    uint32_t outputOffset{0};
    uint32_t commandIndex{0};
  };

  struct ShadowUniform {
    float lightViewProj[16]{};
  };

  struct CullPushConstants {
    uint32_t inputOffset{0};
    uint32_t inputCount{0};
    uint32_t outputOffset{0};
    uint32_t commandIndex{0};
  };

  struct GpuMesh {
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory vertexMemory{VK_NULL_HANDLE};
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory indexMemory{VK_NULL_HANDLE};
    uint32_t indexCount{0};
  };

  struct GpuTexture {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    uint32_t width{0};
    uint32_t height{0};
  };

  struct GpuMaterial {
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkBuffer uniformBuffer{VK_NULL_HANDLE};
    VkDeviceMemory uniformMemory{VK_NULL_HANDLE};
    // Keep track of dependencies to detect changes?
    // For now we rely on AssetRegistry changes.
  };

  std::shared_ptr<VulkanContext> m_context;
  std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
  bool m_ready{false};
  bool m_verboseLogging{true};
  bool m_vsyncEnabled{true};
  float m_timeSeconds{0.0f};
  bool m_waitingForValidExtent{false};
  bool m_shutdown{false};
  bool m_needsSwapchainRecreate{false};
  DebugViewMode m_debugViewMode{DebugViewMode::Final};
  bool m_pickFlipY{false};
  PickRequest m_pendingPick{};
  std::array<PickReadback, kMaxFramesInFlight> m_pickReadbacks{};
  PickResult m_lastPickResult{};
  FrameStats m_lastFrameStats{};
  std::array<FrameStats, kMaxFramesInFlight> m_frameStats{};
  std::array<VkQueryPool, kMaxFramesInFlight> m_queryPools{};
  float m_timestampPeriod{0.0f};
  bool m_timestampsSupported{false};
  std::vector<DeferredDeletion> m_deferredDeletions;

  // Camera state
  float m_cameraX{0.0f};
  float m_cameraY{0.0f};
  float m_cameraZ{0.0f};
  float m_cameraYawDeg{30.0f};
  float m_cameraPitchDeg{25.0f};
  float m_cameraZoom{1.0f};
  float m_cameraDistance{5.0f};

  void *m_nativeHandle{nullptr};
  int m_surfaceWidth{0};
  int m_surfaceHeight{0};

#ifdef __APPLE__
  void *m_metalLayer{nullptr}; // Retained CAMetalLayer for macOS.
#endif

  VkSurfaceKHR m_surface{VK_NULL_HANDLE};
  VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
  VkFormat m_swapchainFormat{VK_FORMAT_UNDEFINED};
  VkExtent2D m_swapchainExtent{};
  VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
  VkFormat m_sceneColorFormat{VK_FORMAT_UNDEFINED};
  VkFormat m_pickingFormat{VK_FORMAT_UNDEFINED};
  bool m_pickingFormatIsUint{false};

  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;

  VkRenderPass m_sceneRenderPass{VK_NULL_HANDLE};
  VkRenderPass m_postProcessRenderPass{VK_NULL_HANDLE};
  VkRenderPass m_pickingRenderPass{VK_NULL_HANDLE};
  VkRenderPass m_shadowRenderPass{VK_NULL_HANDLE};

  VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_textureDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_postProcessDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_shadowDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_shadowSampleDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_cullDescriptorSetLayout{VK_NULL_HANDLE};
  std::array<VkDescriptorPool, kMaxFramesInFlight> m_descriptorPools{};
  VkDescriptorPool m_cullDescriptorPool{VK_NULL_HANDLE};
  VkDescriptorPool m_shadowDescriptorPool{VK_NULL_HANDLE};
  std::vector<VkDescriptorPool> m_textureDescriptorPools;
  size_t m_activeTextureDescriptorPool{0};
  std::array<VkDescriptorSet, kMaxFramesInFlight> m_descriptorSets{};
  std::array<VkDescriptorSet, kMaxFramesInFlight> m_postProcessDescriptorSets{};
  std::array<VkDescriptorSet, kMaxFramesInFlight> m_cullDescriptorSets{};
  std::array<VkDescriptorSet, kMaxFramesInFlight>
      m_shadowSamplerDescriptorSets{};
  std::array<std::array<VkDescriptorSet, kShadowCascadeCount>,
             kMaxFramesInFlight>
      m_shadowCascadeDescriptorSets{};

  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
  VkPipelineLayout m_postProcessPipelineLayout{VK_NULL_HANDLE};
  VkPipelineLayout m_shadowPipelineLayout{VK_NULL_HANDLE};
  VkPipelineLayout m_cullPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkPipeline m_particlePipelineAlpha{VK_NULL_HANDLE};
  VkPipeline m_particlePipelineAdditive{VK_NULL_HANDLE};

  std::vector<VkDescriptorPool> m_materialDescriptorPools;
  size_t m_activeMaterialDescriptorPool{0};
  VkPipeline m_linePipeline{VK_NULL_HANDLE};
  VkPipeline m_overlayPipeline{VK_NULL_HANDLE};
  VkPipeline m_pickingPipeline{VK_NULL_HANDLE};
  VkPipeline m_pickingPipelineUint{VK_NULL_HANDLE};
  VkPipeline m_postProcessPipeline{VK_NULL_HANDLE};
  VkPipeline m_postProcessPipelineUint{VK_NULL_HANDLE};
  VkPipeline m_shadowPipeline{VK_NULL_HANDLE};
  VkPipeline m_cullPipeline{VK_NULL_HANDLE};

  std::vector<VkFramebuffer> m_framebuffers;
  std::array<VkFramebuffer, kMaxFramesInFlight> m_sceneFramebuffers{};
  std::array<VkFramebuffer, kMaxFramesInFlight> m_pickingFramebuffers{};
  std::array<std::array<VkFramebuffer, kShadowCascadeCount>, kMaxFramesInFlight>
      m_shadowFramebuffers{};

  std::array<VkImage, kMaxFramesInFlight> m_sceneColorImages{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_sceneColorMemories{};
  std::array<VkImageView, kMaxFramesInFlight> m_sceneColorViews{};
  std::array<VkImage, kMaxFramesInFlight> m_sceneDepthImages{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_sceneDepthMemories{};
  std::array<VkImageView, kMaxFramesInFlight> m_sceneDepthViews{};

  std::array<VkImage, kMaxFramesInFlight> m_pickingImages{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_pickingMemories{};
  std::array<VkImageView, kMaxFramesInFlight> m_pickingViews{};
  std::array<VkImage, kMaxFramesInFlight> m_pickingDepthImages{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_pickingDepthMemories{};
  std::array<VkImageView, kMaxFramesInFlight> m_pickingDepthViews{};
  std::array<VkBuffer, kMaxFramesInFlight> m_pickingReadbackBuffers{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_pickingReadbackMemories{};

  std::array<VkImage, kMaxFramesInFlight> m_shadowImages{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_shadowMemories{};
  std::array<VkImageView, kMaxFramesInFlight> m_shadowArrayViews{};
  std::array<std::array<VkImageView, kShadowCascadeCount>, kMaxFramesInFlight>
      m_shadowCascadeViews{};

  std::array<VkImage, kMaxFramesInFlight> m_taaHistoryImages{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_taaHistoryMemories{};
  std::array<VkImageView, kMaxFramesInFlight> m_taaHistoryViews{};
  std::array<bool, kMaxFramesInFlight> m_taaHistoryValid{};
  bool m_taaEnabledForFrame{false};
  bool m_shadowEnabledForFrame{false};
  uint32_t m_taaJitterIndex{0};

  VkCommandPool m_commandPool{VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> m_commandBuffers;

  VkBuffer m_instanceInputBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_instanceInputMemory{VK_NULL_HANDLE};
  void *m_instanceInputMapped{nullptr};
  VkBuffer m_instanceOutputBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_instanceOutputMemory{VK_NULL_HANDLE};
  VkBuffer m_instanceFallbackBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_instanceFallbackMemory{VK_NULL_HANDLE};
  VkBuffer m_indirectCommandBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_indirectCommandMemory{VK_NULL_HANDLE};
  void *m_indirectCommandMapped{nullptr};
  size_t m_instanceCapacity{0};
  size_t m_batchCapacity{0};
  std::vector<InstanceData> m_instanceStaging;
  std::vector<DrawBatch> m_drawBatches;

  VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_vertexMemory{VK_NULL_HANDLE};
  VkBuffer m_indexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_indexMemory{VK_NULL_HANDLE};
  uint32_t m_defaultIndexCount{0};
  GpuMesh m_iconMesh{};
  VkBuffer m_lineVertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_lineVertexMemory{VK_NULL_HANDLE};
  uint32_t m_lineVertexCount{0};
  VkBuffer m_selectionVertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_selectionVertexMemory{VK_NULL_HANDLE};
  uint32_t m_selectionVertexCount{0};
  VkBuffer m_lightGizmoVertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_lightGizmoVertexMemory{VK_NULL_HANDLE};
  uint32_t m_lightGizmoVertexCount{0};
  VkBuffer m_colliderVertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_colliderVertexMemory{VK_NULL_HANDLE};
  uint32_t m_colliderVertexCount{0};
  VkBuffer m_particleVertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory m_particleVertexMemory{VK_NULL_HANDLE};
  uint32_t m_particleVertexCount{0};
  uint32_t m_particleAlphaVertexCount{0};
  uint32_t m_particleAdditiveVertexCount{0};
  VkSampler m_textureSampler{VK_NULL_HANDLE};
  VkSampler m_postProcessSampler{VK_NULL_HANDLE};
  VkSampler m_shadowSampler{VK_NULL_HANDLE};
  GpuTexture m_defaultAlbedoTexture{};
  GpuTexture m_defaultNormalTexture{};
  GpuTexture m_defaultMetallicRoughnessTexture{};
  GpuTexture m_defaultEmissiveTexture{};
  GpuTexture m_defaultOcclusionTexture{};
  GpuMaterial m_defaultMaterial{};
  std::array<VkBuffer, kMaxFramesInFlight> m_uniformBuffers{};
  std::array<VkDeviceMemory, kMaxFramesInFlight> m_uniformMemories{};
  std::array<void *, kMaxFramesInFlight> m_uniformMapped{};
  std::array<std::array<VkBuffer, kShadowCascadeCount>, kMaxFramesInFlight>
      m_shadowUniformBuffers{};
  std::array<std::array<VkDeviceMemory, kShadowCascadeCount>,
             kMaxFramesInFlight>
      m_shadowUniformMemories{};
  std::array<std::array<void *, kShadowCascadeCount>, kMaxFramesInFlight>
      m_shadowUniformMapped{};
  uint32_t m_frameIndex{0};
  std::vector<VkSemaphore> m_imageAvailable;
  // Must be per-swapchain-image (present may outlive per-frame fences).
  std::vector<VkSemaphore> m_renderFinishedPerImage;
  std::vector<VkFence> m_inFlight;
  std::vector<VkFence> m_imagesInFlight;
  std::unordered_map<std::string, GpuMesh> m_meshCache;
  std::unordered_set<std::string> m_missingMeshes;
  std::unordered_map<std::string, GpuTexture> m_textureCache;
  std::unordered_set<std::string> m_missingTextures;
  std::unordered_map<std::string, GpuMaterial> m_materialCache;
  std::unordered_set<std::string> m_missingMaterials;

  VkDescriptorSetLayout m_materialDescriptorSetLayout{VK_NULL_HANDLE};

  void CreateSurface(void *nativeHandle);
  void CreateSwapchain(int width, int height);
  void DestroySwapchain();
  void DestroySwapchainResources();
  void DestroyDeviceResources();
  void DestroySurface();
  void RecreateRenderer(int width, int height);
  bool TryRecoverSwapchain();

  void CreateRenderPass();
  void CreateDescriptorSetLayout();
  void CreateMeshBuffers();
  void CreateLineBuffers();
  void CreateSceneResources();
  void CreatePickingResources();
  void CreateShadowResources();
  void CreateUniformBuffers();
  void CreateDescriptorPoolAndSets();
  void CreateTextureDescriptorPool();
  VkDescriptorPool CreateTextureDescriptorPoolInternal();
  void CreateMaterialDescriptorPool();
  VkDescriptorPool CreateMaterialDescriptorPoolInternal();
  void CreateTextureResources();
  void CreatePipeline();
  void CreateFramebuffers();
  void UpdatePostProcessDescriptorSets();
  void UpdateShadowDescriptorSets();
  void UpdateCullDescriptorSets();
  void CreateInstanceBuffers(size_t instanceCount, size_t batchCount);

  void CreateCommandPoolAndBuffers();
  void RecordCommandBuffer(uint32_t imageIndex,
                           const std::vector<DrawInstance> &instances);
  void RecordShadowPass(VkCommandBuffer cb);
  void DispatchCullingPass(VkCommandBuffer cb);
  void RecordOpaquePass(VkCommandBuffer cb,
                        const std::vector<DrawInstance> &instances);
  void RecordPickingPass(VkCommandBuffer cb,
                         const std::vector<DrawInstance> &instances);
  void RecordPostProcessPass(VkCommandBuffer cb, uint32_t imageIndex);
  void CopyTaaHistory(VkCommandBuffer cb, uint32_t imageIndex);
  void RecordOverlayPass(VkCommandBuffer cb);
  void UpdateUniformBuffer(uint32_t frameIndex, const RenderView &view);
  void PrepareInstanceData(const std::vector<DrawInstance> &instances);
  void UpdateSelectionBuffer(const std::vector<DrawInstance> &instances,
                             const RenderView &view);
  void UpdateLightGizmoBuffer(const RenderView &view);
  void UpdateColliderBuffer(const RenderView &view);
  void UpdateParticleBuffer(const RenderView &view);
  void DestroyMeshCache();
  void DestroyTextureCache();
  void DestroyMaterialCache();
  void DestroySceneResources();
  void DestroyPickingResources();
  void DestroyShadowResources();
  void ProcessDeferredDeletions();
  void EnqueueDeletion(std::function<void()> &&callback,
                       uint32_t frames = kMaxFramesInFlight);
  void FlushDeferredDeletions();

#ifdef __APPLE__
  void UpdateMetalLayerSize(int width, int height);
#endif

  void CreateSyncObjects();
  void CreateQueryPools();
  [[nodiscard]] std::vector<DrawInstance>
  InstancesFromView(const RenderView &view, float timeSeconds) const;

  enum class TextureUsage {
    Albedo,
    Normal,
    MetallicRoughness,
    Emissive,
    Occlusion,
  };

  [[nodiscard]] const GpuMesh *ResolveMesh(const std::string &assetId);
  [[nodiscard]] const GpuTexture *ResolveTexture(const std::string &assetId,
                                                 TextureUsage usage);
  [[nodiscard]] const GpuMaterial *ResolveMaterial(const std::string &assetId);
  GpuMaterial CreateMaterialResources(const Assets::Material &material);
  GpuTexture CreateTextureFromPixels(const unsigned char *pixels,
                                     uint32_t width, uint32_t height,
                                     bool srgb);
  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout);
  void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height);

  [[nodiscard]] std::string ShaderPath(const char *filename) const;
  [[nodiscard]] std::vector<char> ReadFileBinary(const std::string &path) const;
  [[nodiscard]] VkShaderModule
  CreateShaderModule(const std::vector<char> &code) const;
};
} // namespace Aetherion::Rendering
