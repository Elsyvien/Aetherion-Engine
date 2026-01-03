#pragma once

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene {
class AudioListenerComponent : public Component {
public:
  AudioListenerComponent() = default;
  ~AudioListenerComponent() override = default;

  std::string GetDisplayName() const override { return "Audio Listener"; }

  // Future: Add properties like MasterVolume, gain, etc. specific to listener?
  // For now, it uses the Transform of the entity.

  bool IsActive() const { return m_Active; }
  void SetActive(bool active) { m_Active = active; }

private:
  bool m_Active = true;
};
} // namespace Aetherion::Scene
