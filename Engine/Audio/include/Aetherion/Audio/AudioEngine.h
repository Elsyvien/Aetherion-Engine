#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declaration for miniaudio types to avoid including the heavy header
// here if possible, but miniaudio is a single header, so usually we include it
// in cpp or use a void* implementation detail. For simplicity in this plan,
// we'll use a PIMPL or opaque pointer approach if we wanted to hide it, but
// since miniaudio is C-style, we can just forward declare ma_engine and
// ma_sound.
struct ma_engine;
struct ma_sound;

namespace Aetherion::Audio {
class AudioEngine {
public:
  AudioEngine() = default;
  ~AudioEngine() = default;

  AudioEngine(const AudioEngine &) = delete;
  AudioEngine &operator=(const AudioEngine &) = delete;

  void Initialize();
  void Shutdown();
  void Update();

  void PlayOneShot(const std::filesystem::path &path, float volume = 1.0f);

  ma_engine *GetNativeEngine() const { return m_Engine; }

private:
  ma_engine *m_Engine = nullptr;
  bool m_Initialized = false;
};
}

ma_engine *m_Engine = nullptr;
bool m_Initialized = false;
}
;
} // namespace Aetherion::Audio
