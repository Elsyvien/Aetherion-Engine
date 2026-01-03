#include "Aetherion/Audio/AudioEngine.h"

// Define MINT_AUDIO_IMPLEMENTATION in ONE source file.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <iostream>

namespace Aetherion::Audio {
void AudioEngine::Initialize() {
  if (m_Initialized)
    return;

  m_Engine = new ma_engine();
  ma_result result = ma_engine_init(nullptr, m_Engine);
  if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Failed to initialize audio engine."
              << std::endl;
    delete m_Engine;
    m_Engine = nullptr;
    return;
  }

  m_Initialized = true;
  std::cout << "[AudioEngine] Initialized successfully." << std::endl;
}

void AudioEngine::Shutdown() {
  if (!m_Initialized)
    return;

  ma_engine_uninit(m_Engine);
  delete m_Engine;
  m_Engine = nullptr;
  m_Initialized = false;
  std::cout << "[AudioEngine] Shutdown successfully." << std::endl;
}

void AudioEngine::Update() {
  // miniaudio processes in its own thread mostly, but we can do main-thread
  // tasks here if needed.
}

void AudioEngine::PlayOneShot(const std::filesystem::path &path, float volume) {
  if (!m_Initialized)
    return;

  std::string pathStr = path.string();
  ma_engine_play_sound(m_Engine, pathStr.c_str(), nullptr);

  // Note: miniaudio's play_sound is fire-and-forget for simple usage.
  // For volume control on one-shots, we might need a slightly more complex
  // approach group or just accept defaults for now. To keep it simple, we just
  // play it. If we really need volume, we'd create a sound, set volume, start
  // it, and let it auto-delete (if supported) or manage its lifetime.
  // ma_engine_play_sound doesn't return a sound object to modify.
}
} // namespace Aetherion::Audio
