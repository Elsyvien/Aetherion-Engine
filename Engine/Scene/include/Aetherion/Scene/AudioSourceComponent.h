#pragma once

#include "Aetherion/Scene/Component.h"
#include <string>

namespace Aetherion::Scene {
class AudioSourceComponent : public Component {
public:
  AudioSourceComponent();
  ~AudioSourceComponent() override = default;

  std::string GetDisplayName() const override;

  void SetSoundPath(const std::string &path) { m_SoundPath = path; }
  const std::string &GetSoundPath() const { return m_SoundPath; }

  void SetVolume(float volume) { m_Volume = volume; }
  float GetVolume() const { return m_Volume; }

  void SetPitch(float pitch) { m_Pitch = pitch; }
  float GetPitch() const { return m_Pitch; }

  void SetLoop(bool loop) { m_Loop = loop; }
  bool GetLoop() const { return m_Loop; }

  void SetSpatial(bool spatial) { m_Spatial = spatial; }
  bool GetSpatial() const { return m_Spatial; }

  void SetPlayOnAwake(bool playOnAwake) { m_PlayOnAwake = playOnAwake; }
  bool GetPlayOnAwake() const { return m_PlayOnAwake; }

  // Simple playback triggers (called by systems or editor)
  void Play();
  void Stop();

private:
  std::string m_SoundPath;
  float m_Volume = 1.0f;
  float m_Pitch = 1.0f;
  bool m_Loop = false;
  bool m_Spatial = true;
  bool m_PlayOnAwake = true;
};
} // namespace Aetherion::Scene
