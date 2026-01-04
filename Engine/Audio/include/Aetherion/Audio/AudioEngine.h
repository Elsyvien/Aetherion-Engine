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

/// @brief Handle to a playing sound instance
struct SoundHandle {
  uint32_t id{0};
  uint32_t generation{0};

  bool IsValid() const noexcept { return generation != 0; }
  bool operator==(const SoundHandle &other) const noexcept {
    return id == other.id && generation == other.generation;
  }
};

/// @brief Settings for sound playback
struct SoundSettings {
  float volume{1.0f};        ///< Volume multiplier (0.0 - 1.0+)
  float pitch{1.0f};         ///< Pitch multiplier (0.5 = half speed, 2.0 = double)
  float pan{0.0f};           ///< Stereo pan (-1.0 = left, 0.0 = center, 1.0 = right)
  bool loop{false};          ///< Whether to loop the sound
  bool streaming{false};     ///< Stream from disk instead of loading into memory
  bool startPaused{false};   ///< Start in paused state
};

/// @brief Settings for 3D spatialized audio
struct SpatialSettings {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 velocity{0.0f, 0.0f, 0.0f};  ///< For Doppler effect
  float minDistance{1.0f};    ///< Distance at which attenuation begins
  float maxDistance{100.0f};  ///< Distance at which sound is inaudible
  float rolloff{1.0f};        ///< Attenuation rolloff factor
  bool spatialized{false};    ///< Enable 3D spatialization
};

/// @brief State of a sound
enum class SoundState : uint8_t {
  Stopped = 0,
  Playing = 1,
  Paused = 2,
  Finished = 3
};

class AudioEngine {
public:
  AudioEngine() = default;
  ~AudioEngine() = default;

  AudioEngine(const AudioEngine &) = delete;
  AudioEngine &operator=(const AudioEngine &) = delete;

  void Initialize();
  void Shutdown();
  void Update();

  /// @brief Play a fire-and-forget sound (legacy API)
  void PlayOneShot(const std::filesystem::path &path, float volume = 1.0f);

  /// @brief Play a sound with full control
  /// @param path Path to the audio file
  /// @param settings Playback settings
  /// @return Handle to the sound instance
  SoundHandle Play(const std::filesystem::path &path,
                   const SoundSettings &settings = {});

  /// @brief Play a 3D spatialized sound
  /// @param path Path to the audio file
  /// @param settings Playback settings
  /// @param spatial 3D spatial settings
  /// @return Handle to the sound instance
  SoundHandle PlaySpatial(const std::filesystem::path &path,
                          const SoundSettings &settings,
                          const SpatialSettings &spatial);

  /// @brief Stop a sound
  void Stop(SoundHandle handle);

  /// @brief Pause a sound
  void Pause(SoundHandle handle);

  /// @brief Resume a paused sound
  void Resume(SoundHandle handle);

  /// @brief Check if a sound is playing
  [[nodiscard]] bool IsPlaying(SoundHandle handle) const;

  /// @brief Get the current state of a sound
  [[nodiscard]] SoundState GetState(SoundHandle handle) const;

  /// @brief Set volume of a playing sound
  void SetVolume(SoundHandle handle, float volume);

  /// @brief Set pitch of a playing sound
  void SetPitch(SoundHandle handle, float pitch);

  /// @brief Set pan of a playing sound (-1 to 1)
  void SetPan(SoundHandle handle, float pan);

  /// @brief Set looping state of a playing sound
  void SetLooping(SoundHandle handle, bool loop);

  /// @brief Update 3D position of a spatialized sound
  void SetPosition(SoundHandle handle, const glm::vec3 &position);

  /// @brief Update listener position and orientation
  void SetListenerPosition(const glm::vec3 &position,
                           const glm::vec3 &forward,
                           const glm::vec3 &up);

  /// @brief Set master volume
  void SetMasterVolume(float volume);

  /// @brief Get master volume
  [[nodiscard]] float GetMasterVolume() const noexcept { return m_masterVolume; }

  /// @brief Get the native miniaudio engine (for advanced usage)
  ma_engine *GetNativeEngine() const { return m_Engine; }

  /// @brief Get statistics about audio system
  struct Stats {
    uint32_t activeSounds{0};
    uint32_t totalSoundsPlayed{0};
  };
  [[nodiscard]] Stats GetStats() const noexcept;

private:
  struct SoundEntry {
    ma_sound *sound{nullptr};
    uint32_t generation{0};
    bool inUse{false};
    bool isSpatialized{false};
    std::filesystem::path sourcePath;
  };

  [[nodiscard]] SoundEntry *GetSoundEntry(SoundHandle handle);
  [[nodiscard]] const SoundEntry *GetSoundEntry(SoundHandle handle) const;
  void CleanupFinishedSounds();

  ma_engine *m_Engine = nullptr;
  bool m_Initialized = false;
  float m_masterVolume = 1.0f;

  std::vector<SoundEntry> m_sounds;
  std::vector<uint32_t> m_freeIndices;
  uint32_t m_nextGeneration{1};
  uint32_t m_totalSoundsPlayed{0};
};
} // namespace Aetherion::Audio
