#include "Aetherion/Scene/AudioSourceComponent.h"

namespace Aetherion::Scene {
AudioSourceComponent::AudioSourceComponent() = default;

std::string AudioSourceComponent::GetDisplayName() const {
  return "Audio Source";
}

void AudioSourceComponent::Play() { m_PlayRequested = true; }

void AudioSourceComponent::Stop() {
  // Not supported in this simple one-shot implementation yet.
}
} // namespace Aetherion::Scene
