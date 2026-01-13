#pragma once

#include "Aetherion/Audio/AudioEngine.h"
#include "Aetherion/Core/Types.h"
#include "Aetherion/Scene/System.h"
#include <memory>
#include <unordered_map>


namespace Aetherion::Scene {
class Scene;
}
namespace Aetherion::Assets {
class AssetRegistry;
}

namespace Aetherion::Audio {

/// @brief System that manages audio playback for scene entities
///
/// The AudioSystem is responsible for:
/// - Playing sounds when AudioSourceComponent requests playback
/// - Updating 3D spatial positions for spatialized sounds
/// - Managing sound lifetimes tied to entities
class AudioSystem {
public:
  explicit AudioSystem(AudioEngine *engine,
                       Assets::AssetRegistry *registry = nullptr);
  ~AudioSystem();

  void BindScene(Scene::Scene *scene);
  void UnbindScene();
  void Update(float dt);
  void Shutdown();
  void SetAssetRegistry(Assets::AssetRegistry *registry) {
    m_assetRegistry = registry;
  }

  /// @brief Stop all sounds for an entity
  void StopEntity(Core::EntityId entityId);

  /// @brief Pause all sounds for an entity
  void PauseEntity(Core::EntityId entityId);

  /// @brief Resume all sounds for an entity
  void ResumeEntity(Core::EntityId entityId);

  /// @brief Set whether the audio system is enabled
  void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
  [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

private:
  void UpdateListenerPosition();
  void UpdateSpatialSounds();
  void CleanupStoppedSounds();

  AudioEngine *m_Engine = nullptr;
  Assets::AssetRegistry *m_assetRegistry = nullptr;
  Scene::Scene *m_Scene = nullptr;
  bool m_enabled{true};

  /// @brief Active sound handles per entity
  struct EntitySound {
    SoundHandle handle;
    bool spatialized{false};
  };
  std::unordered_map<Core::EntityId, std::vector<EntitySound>> m_entitySounds;
};
} // namespace Aetherion::Audio
