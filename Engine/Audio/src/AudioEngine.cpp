#include "Aetherion/Audio/AudioEngine.h"

// Define MINT_AUDIO_IMPLEMENTATION in ONE source file.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <iostream>

namespace Aetherion::Audio {
void AudioEngine::Initialize() {
  if (m_Initialized)
    return;

  m_Engine = new ma_engine();
  ma_engine_config config = ma_engine_config_init();
  config.listenerCount = 1; // Enable 3D audio listener

  ma_result result = ma_engine_init(&config, m_Engine);
  if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Failed to initialize audio engine."
              << std::endl;
    delete m_Engine;
    m_Engine = nullptr;
    return;
  }

  m_sounds.reserve(64);
  m_Initialized = true;
  std::cout << "[AudioEngine] Initialized successfully." << std::endl;
}

void AudioEngine::Shutdown() {
  if (!m_Initialized)
    return;

  // Stop and cleanup all sounds
  for (auto &entry : m_sounds) {
    if (entry.inUse && entry.sound) {
      ma_sound_uninit(entry.sound);
      delete entry.sound;
      entry.sound = nullptr;
      entry.inUse = false;
    }
  }
  m_sounds.clear();
  m_freeIndices.clear();

  ma_engine_uninit(m_Engine);
  delete m_Engine;
  m_Engine = nullptr;
  m_Initialized = false;
  std::cout << "[AudioEngine] Shutdown successfully." << std::endl;
}

void AudioEngine::Update() {
  if (!m_Initialized)
    return;

  // Clean up finished sounds
  CleanupFinishedSounds();
}

void AudioEngine::CleanupFinishedSounds() {
  for (size_t i = 0; i < m_sounds.size(); ++i) {
    auto &entry = m_sounds[i];
    if (entry.inUse && entry.sound) {
      if (ma_sound_at_end(entry.sound) && !ma_sound_is_looping(entry.sound)) {
        ma_sound_uninit(entry.sound);
        delete entry.sound;
        entry.sound = nullptr;
        entry.inUse = false;
        m_freeIndices.push_back(static_cast<uint32_t>(i));
      }
    }
  }
}

void AudioEngine::PlayOneShot(const std::filesystem::path &path, float volume) {
  if (!m_Initialized)
    return;

  std::string pathStr = path.string();
  ma_engine_play_sound(m_Engine, pathStr.c_str(), nullptr);
  m_totalSoundsPlayed++;
}

SoundHandle AudioEngine::Play(const std::filesystem::path &path,
                               const SoundSettings &settings) {
  if (!m_Initialized) {
    return SoundHandle{};
  }

  // Create sound
  ma_sound *sound = new ma_sound();
  ma_uint32 flags = 0;
  if (settings.streaming) {
    flags |= MA_SOUND_FLAG_STREAM;
  }
  if (!settings.startPaused) {
    // Don't auto-start; we'll start after setting parameters
  }

  std::string pathStr = path.string();
  ma_result result = ma_sound_init_from_file(m_Engine, pathStr.c_str(), flags,
                                             nullptr, nullptr, sound);
  if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Failed to load sound: " << pathStr << std::endl;
    delete sound;
    return SoundHandle{};
  }

  // Apply settings
  ma_sound_set_volume(sound, settings.volume * m_masterVolume);
  ma_sound_set_pitch(sound, settings.pitch);
  ma_sound_set_pan(sound, settings.pan);
  ma_sound_set_looping(sound, settings.loop ? MA_TRUE : MA_FALSE);

  // Find or create slot
  uint32_t index;
  if (!m_freeIndices.empty()) {
    index = m_freeIndices.back();
    m_freeIndices.pop_back();
  } else {
    index = static_cast<uint32_t>(m_sounds.size());
    m_sounds.push_back({});
  }

  auto &entry = m_sounds[index];
  entry.sound = sound;
  entry.generation = m_nextGeneration++;
  entry.inUse = true;
  entry.isSpatialized = false;
  entry.sourcePath = path;

  // Start playback if not paused
  if (!settings.startPaused) {
    ma_sound_start(sound);
  }

  m_totalSoundsPlayed++;
  return SoundHandle{index, entry.generation};
}

SoundHandle AudioEngine::PlaySpatial(const std::filesystem::path &path,
                                      const SoundSettings &settings,
                                      const SpatialSettings &spatial) {
  if (!m_Initialized) {
    return SoundHandle{};
  }

  // Create sound
  ma_sound *sound = new ma_sound();
  ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION; // We'll enable it manually
  if (settings.streaming) {
    flags |= MA_SOUND_FLAG_STREAM;
  }
  flags &= ~MA_SOUND_FLAG_NO_SPATIALIZATION; // Enable spatialization

  std::string pathStr = path.string();
  ma_result result = ma_sound_init_from_file(m_Engine, pathStr.c_str(), flags,
                                             nullptr, nullptr, sound);
  if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Failed to load spatial sound: " << pathStr
              << std::endl;
    delete sound;
    return SoundHandle{};
  }

  // Apply settings
  ma_sound_set_volume(sound, settings.volume * m_masterVolume);
  ma_sound_set_pitch(sound, settings.pitch);
  ma_sound_set_looping(sound, settings.loop ? MA_TRUE : MA_FALSE);

  // Apply 3D settings
  if (spatial.spatialized) {
    ma_sound_set_spatialization_enabled(sound, MA_TRUE);
    ma_sound_set_position(sound, spatial.position.x, spatial.position.y,
                          spatial.position.z);
    ma_sound_set_velocity(sound, spatial.velocity.x, spatial.velocity.y,
                          spatial.velocity.z);
    ma_sound_set_min_distance(sound, spatial.minDistance);
    ma_sound_set_max_distance(sound, spatial.maxDistance);
    ma_sound_set_rolloff(sound, spatial.rolloff);
  } else {
    ma_sound_set_spatialization_enabled(sound, MA_FALSE);
  }

  // Find or create slot
  uint32_t index;
  if (!m_freeIndices.empty()) {
    index = m_freeIndices.back();
    m_freeIndices.pop_back();
  } else {
    index = static_cast<uint32_t>(m_sounds.size());
    m_sounds.push_back({});
  }

  auto &entry = m_sounds[index];
  entry.sound = sound;
  entry.generation = m_nextGeneration++;
  entry.inUse = true;
  entry.isSpatialized = spatial.spatialized;
  entry.sourcePath = path;

  // Start playback if not paused
  if (!settings.startPaused) {
    ma_sound_start(sound);
  }

  m_totalSoundsPlayed++;
  return SoundHandle{index, entry.generation};
}

void AudioEngine::Stop(SoundHandle handle) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_stop(entry->sound);
  ma_sound_uninit(entry->sound);
  delete entry->sound;
  entry->sound = nullptr;
  entry->inUse = false;
  m_freeIndices.push_back(handle.id);
}

void AudioEngine::Pause(SoundHandle handle) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_stop(entry->sound);
}

void AudioEngine::Resume(SoundHandle handle) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_start(entry->sound);
}

bool AudioEngine::IsPlaying(SoundHandle handle) const {
  const auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return false;

  return ma_sound_is_playing(entry->sound) == MA_TRUE;
}

SoundState AudioEngine::GetState(SoundHandle handle) const {
  const auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return SoundState::Stopped;

  if (ma_sound_at_end(entry->sound)) {
    return SoundState::Finished;
  }
  if (ma_sound_is_playing(entry->sound)) {
    return SoundState::Playing;
  }
  return SoundState::Paused;
}

void AudioEngine::SetVolume(SoundHandle handle, float volume) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_set_volume(entry->sound, volume * m_masterVolume);
}

void AudioEngine::SetPitch(SoundHandle handle, float pitch) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_set_pitch(entry->sound, pitch);
}

void AudioEngine::SetPan(SoundHandle handle, float pan) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_set_pan(entry->sound, pan);
}

void AudioEngine::SetLooping(SoundHandle handle, bool loop) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound)
    return;

  ma_sound_set_looping(entry->sound, loop ? MA_TRUE : MA_FALSE);
}

void AudioEngine::SetPosition(SoundHandle handle, const glm::vec3 &position) {
  auto *entry = GetSoundEntry(handle);
  if (!entry || !entry->sound || !entry->isSpatialized)
    return;

  ma_sound_set_position(entry->sound, position.x, position.y, position.z);
}

void AudioEngine::SetListenerPosition(const glm::vec3 &position,
                                       const glm::vec3 &forward,
                                       const glm::vec3 &up) {
  if (!m_Initialized || !m_Engine)
    return;

  ma_engine_listener_set_position(m_Engine, 0, position.x, position.y,
                                  position.z);
  ma_engine_listener_set_direction(m_Engine, 0, forward.x, forward.y,
                                   forward.z);
  ma_engine_listener_set_world_up(m_Engine, 0, up.x, up.y, up.z);
}

void AudioEngine::SetMasterVolume(float volume) {
  m_masterVolume = volume;
  if (m_Initialized && m_Engine) {
    ma_engine_set_volume(m_Engine, volume);
  }
}

AudioEngine::Stats AudioEngine::GetStats() const noexcept {
  Stats stats;
  stats.totalSoundsPlayed = m_totalSoundsPlayed;
  for (const auto &entry : m_sounds) {
    if (entry.inUse && entry.sound && ma_sound_is_playing(entry.sound)) {
      stats.activeSounds++;
    }
  }
  return stats;
}

AudioEngine::SoundEntry *AudioEngine::GetSoundEntry(SoundHandle handle) {
  if (handle.id >= m_sounds.size()) {
    return nullptr;
  }
  auto &entry = m_sounds[handle.id];
  if (!entry.inUse || entry.generation != handle.generation) {
    return nullptr;
  }
  return &entry;
}

const AudioEngine::SoundEntry *
AudioEngine::GetSoundEntry(SoundHandle handle) const {
  if (handle.id >= m_sounds.size()) {
    return nullptr;
  }
  const auto &entry = m_sounds[handle.id];
  if (!entry.inUse || entry.generation != handle.generation) {
    return nullptr;
  }
  return &entry;
}

} // namespace Aetherion::Audio
