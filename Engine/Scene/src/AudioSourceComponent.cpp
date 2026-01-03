#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Audio/AudioEngine.h"

namespace Aetherion::Scene {
AudioSourceComponent::AudioSourceComponent() = default;

std::string AudioSourceComponent::GetDisplayName() const {
  return "Audio Source";
}

void AudioSourceComponent::Play() {
  // Playback is now handled by the AudioSystem iterating over components
  // or by explicitly passing the engine to this method.
  // For now, this is a data container.
}

void AudioSourceComponent::Stop() {
  // Not supported in this simple one-shot implementation yet.
}
} // namespace Aetherion::Scene
