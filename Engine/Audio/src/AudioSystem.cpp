#include "Aetherion/Audio/AudioSystem.h"
#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Scene/AudioListenerComponent.h"
#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"
#include <filesystem>

namespace Aetherion::Audio {
AudioSystem::AudioSystem(AudioEngine *engine,
                         Assets::AssetRegistry *registry)
    : m_Engine(engine), m_assetRegistry(registry) {}

AudioSystem::~AudioSystem() { Shutdown(); }

void AudioSystem::BindScene(Scene::Scene *scene) {
  if (m_Scene == scene) {
    return;
  }
  UnbindScene();
  m_Scene = scene;
}

void AudioSystem::UnbindScene() {
  // Stop all entity sounds
  if (m_Engine) {
    for (auto &[entityId, sounds] : m_entitySounds) {
      for (auto &es : sounds) {
        m_Engine->Stop(es.handle);
      }
    }
  }
  m_entitySounds.clear();
  m_Scene = nullptr;
}

void AudioSystem::Update(float dt) {
  if (!m_Scene || !m_Engine || !m_enabled)
    return;

  // Update listener position first
  UpdateListenerPosition();

  // Clean up finished sounds
  CleanupStoppedSounds();

  const auto &entities = m_Scene->GetEntities();
  for (const auto &entity : entities) {
    if (auto audio = entity->GetComponent<Scene::AudioSourceComponent>()) {
      Core::EntityId entityId = entity->GetId();

      // Handle PlayOnAwake
      if (audio->GetPlayOnAwake() && !audio->HasAwakePlayed()) {
        audio->Play(); // This sets PlayRequested
        audio->SetAwakePlayed(true);
      }

      // Handle Play Request
      if (audio->IsPlayRequested()) {
        const std::string &soundId = audio->GetSoundPath();
        if (!soundId.empty()) {
          auto resolvePath = [this](const std::string &id) {
            if (id.empty()) {
              return std::filesystem::path{};
            }
            if (m_assetRegistry) {
              if (const auto *entry = m_assetRegistry->FindEntry(id)) {
                return entry->path;
              }
              const auto root = m_assetRegistry->GetRootPath();
              if (!root.empty()) {
                return root / id;
              }
            }
            return std::filesystem::path(id);
          };

          const std::filesystem::path resolvedPath = resolvePath(soundId);
          if (!resolvedPath.empty()) {
            SoundSettings settings;
            settings.volume = audio->GetVolume();
            settings.pitch = audio->GetPitch();
            settings.loop = audio->GetLoop();
            settings.streaming = false; // Could be configurable

            SoundHandle handle;
            if (audio->GetSpatial()) {
              // Get entity position for 3D audio
              auto transform =
                  entity->GetComponent<Scene::TransformComponent>();
              SpatialSettings spatial;
              spatial.spatialized = true;
              if (transform) {
                spatial.position = glm::vec3(transform->GetPositionX(),
                                             transform->GetPositionY(),
                                             transform->GetPositionZ());
              }
              handle = m_Engine->PlaySpatial(resolvedPath, settings, spatial);
            } else {
              handle = m_Engine->Play(resolvedPath, settings);
            }

            if (handle.IsValid()) {
              m_entitySounds[entityId].push_back({handle, audio->GetSpatial()});
            }
          }
        }
        audio->ClearPlayRequested();
      }
    }
  }

  // Update spatial positions for active sounds
  UpdateSpatialSounds();
}

void AudioSystem::UpdateListenerPosition() {
  if (!m_Scene || !m_Engine)
    return;

  // Find entity with AudioListenerComponent
  const auto &entities = m_Scene->GetEntities();
  for (const auto &entity : entities) {
    if (entity->HasComponent<Scene::AudioListenerComponent>()) {
      auto transform = entity->GetComponent<Scene::TransformComponent>();
      if (transform) {
        glm::vec3 position(transform->GetPositionX(),
                           transform->GetPositionY(),
                           transform->GetPositionZ());
        // Default forward/up vectors - could be computed from rotation
        glm::vec3 forward(0.0f, 0.0f, -1.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        m_Engine->SetListenerPosition(position, forward, up);
      }
      break; // Only one listener
    }
  }
}

void AudioSystem::UpdateSpatialSounds() {
  if (!m_Scene || !m_Engine)
    return;

  for (auto &[entityId, sounds] : m_entitySounds) {
    auto entity = m_Scene->FindEntityById(entityId);
    if (!entity)
      continue;

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
      continue;

    glm::vec3 position(transform->GetPositionX(),
                       transform->GetPositionY(),
                       transform->GetPositionZ());

    for (auto &es : sounds) {
      if (es.spatialized && m_Engine->IsPlaying(es.handle)) {
        m_Engine->SetPosition(es.handle, position);
      }
    }
  }
}

void AudioSystem::CleanupStoppedSounds() {
  for (auto it = m_entitySounds.begin(); it != m_entitySounds.end();) {
    auto &sounds = it->second;
    sounds.erase(
        std::remove_if(sounds.begin(), sounds.end(),
                       [this](const EntitySound &es) {
                         return m_Engine->GetState(es.handle) == SoundState::Stopped ||
                                m_Engine->GetState(es.handle) == SoundState::Finished;
                       }),
        sounds.end());

    if (sounds.empty()) {
      it = m_entitySounds.erase(it);
    } else {
      ++it;
    }
  }
}

void AudioSystem::StopEntity(Core::EntityId entityId) {
  auto it = m_entitySounds.find(entityId);
  if (it == m_entitySounds.end() || !m_Engine)
    return;

  for (auto &es : it->second) {
    m_Engine->Stop(es.handle);
  }
  m_entitySounds.erase(it);
}

void AudioSystem::PauseEntity(Core::EntityId entityId) {
  auto it = m_entitySounds.find(entityId);
  if (it == m_entitySounds.end() || !m_Engine)
    return;

  for (auto &es : it->second) {
    m_Engine->Pause(es.handle);
  }
}

void AudioSystem::ResumeEntity(Core::EntityId entityId) {
  auto it = m_entitySounds.find(entityId);
  if (it == m_entitySounds.end() || !m_Engine)
    return;

  for (auto &es : it->second) {
    m_Engine->Resume(es.handle);
  }
}

void AudioSystem::Shutdown() {
  UnbindScene();
}
} // namespace Aetherion::Audio
