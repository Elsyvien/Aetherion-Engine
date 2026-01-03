#include "Aetherion/Audio/AudioSystem.h"
#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"

namespace Aetherion::Audio {
AudioSystem::AudioSystem(AudioEngine *engine) : m_Engine(engine) {}

AudioSystem::~AudioSystem() { Shutdown(); }

void AudioSystem::BindScene(Scene::Scene *scene) { m_Scene = scene; }

void AudioSystem::Update(float dt) {
  if (!m_Scene || !m_Engine)
    return;

  const auto &entities = m_Scene->GetEntities();
  for (const auto &entity : entities) {
    if (auto audio = entity->GetComponent<Scene::AudioSourceComponent>()) {
      // Handle PlayOnAwake
      if (audio->GetPlayOnAwake() && !audio->HasAwakePlayed()) {
        audio->Play(); // This sets PlayRequested
        audio->SetAwakePlayed(true);
      }

      // Handle Play Request
      if (audio->IsPlayRequested()) {
        if (!audio->GetSoundPath().empty()) {
          m_Engine->PlayOneShot(audio->GetSoundPath(), audio->GetVolume());
          // Note: Loop, Pitch, Spatial are ignored by PlayOneShot for now.
        }
        audio->ClearPlayRequested();
      }
    }
  }
}

void AudioSystem::Shutdown() {
  // Stop all sounds
}
} // namespace Aetherion::Audio
