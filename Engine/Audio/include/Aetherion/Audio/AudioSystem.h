#pragma once

#include "Aetherion/Audio/AudioEngine.h"
#include "Aetherion/Scene/System.h"
#include <memory>
#include <unordered_map>


namespace Aetherion::Scene {
class Scene;
}

namespace Aetherion::Audio {
class AudioSystem {
public:
  AudioSystem(AudioEngine *engine);
  ~AudioSystem();

  void BindScene(Scene::Scene *scene);
  void Update(float dt);
  void Shutdown();

private:
  AudioEngine *m_Engine = nullptr;
  Scene::Scene *m_Scene = nullptr;

  // Map EntityId to some runtime sound object wrapper
  // For now, let's assume we play sounds via AudioEngine and track them here if
  // we want 3D updates. But since AudioEngine::PlayOneShot is fire-and-forget,
  // we can't update them easily. We need to upgrade AudioEngine to return sound
  // handles or objects.

  // To keep it simple for this step, we will implement PlayOnAwake logic here
  // using OneShot. Real-time updates (moving sounds) require ma_sound
  // management.

  // struct SoundInstance { ma_sound* sound; ... };
  // std::unordered_map<uint32_t, SoundInstance> m_ActiveSounds;
};
} // namespace Aetherion::Audio
