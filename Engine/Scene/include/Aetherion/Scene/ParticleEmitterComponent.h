#pragma once

#include "Aetherion/Scene/Component.h"

#include <array>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace Aetherion::Scene {

/// @brief Shape from which particles are emitted
enum class ParticleEmissionShape { Point = 0, Sphere = 1, Cone = 2, Box = 3 };

/// @brief Blend mode for particle rendering
enum class ParticleBlendMode {
  Alpha = 0,   // Standard alpha blending
  Additive = 1 // Additive blending (fire, sparks)
};

/// @brief Single particle state (CPU-simulated)
struct Particle {
  float position[3]{0.0f, 0.0f, 0.0f};
  float velocity[3]{0.0f, 0.0f, 0.0f};
  float size{1.0f};
  float startSize{1.0f};
  float endSize{0.0f};
  float color[4]{1.0f, 1.0f, 1.0f, 1.0f};      // Current RGBA
  float startColor[4]{1.0f, 1.0f, 1.0f, 1.0f}; // Initial RGBA
  float endColor[4]{1.0f, 1.0f, 1.0f, 0.0f};   // Final RGBA (fades out)
  float rotation{0.0f};
  float rotationSpeed{0.0f};
  float age{0.0f};
  float lifetime{1.0f};
  bool alive{false};
};

/// @brief Particle emitter component for visual effects
///
/// This component manages a pool of particles that are spawned, simulated,
/// and rendered. It supports various emission shapes, color gradients,
/// physics (gravity), and is designed to integrate with AI systems.
///
/// AI Integration Points:
/// - SetPromptHint() for AI-driven effect descriptions
/// - SetBehaviorCallback() for AI-controlled particle behavior
/// - Named presets can be loaded via SetPreset() from AI commands
class ParticleEmitterComponent final : public Component {
public:
  ParticleEmitterComponent();
  ~ParticleEmitterComponent() override = default;

  [[nodiscard]] std::string GetDisplayName() const override {
    return "Particle Emitter";
  }

  // =========================================================================
  // Emission Configuration
  // =========================================================================

  void SetEmissionRate(float particlesPerSecond) noexcept {
    m_emissionRate = particlesPerSecond;
  }
  [[nodiscard]] float GetEmissionRate() const noexcept {
    return m_emissionRate;
  }

  void SetDuration(float seconds) noexcept { m_duration = seconds; }
  [[nodiscard]] float GetDuration() const noexcept { return m_duration; }

  void SetLooping(bool loop) noexcept { m_looping = loop; }
  [[nodiscard]] bool IsLooping() const noexcept { return m_looping; }

  void SetMaxParticles(uint32_t max) noexcept;
  [[nodiscard]] uint32_t GetMaxParticles() const noexcept {
    return m_maxParticles;
  }

  void SetEmissionShape(ParticleEmissionShape shape) noexcept {
    m_emissionShape = shape;
  }
  [[nodiscard]] ParticleEmissionShape GetEmissionShape() const noexcept {
    return m_emissionShape;
  }

  // Shape parameters
  void SetShapeRadius(float radius) noexcept { m_shapeRadius = radius; }
  [[nodiscard]] float GetShapeRadius() const noexcept { return m_shapeRadius; }

  void SetShapeAngle(float degrees) noexcept { m_shapeAngle = degrees; }
  [[nodiscard]] float GetShapeAngle() const noexcept { return m_shapeAngle; }

  void SetShapeExtents(float x, float y, float z) noexcept;
  [[nodiscard]] std::array<float, 3> GetShapeExtents() const noexcept {
    return m_shapeExtents;
  }

  // =========================================================================
  // Particle Properties
  // =========================================================================

  void SetLifetimeRange(float minSec, float maxSec) noexcept;
  [[nodiscard]] float GetMinLifetime() const noexcept { return m_minLifetime; }
  [[nodiscard]] float GetMaxLifetime() const noexcept { return m_maxLifetime; }

  void SetSpeedRange(float minSpeed, float maxSpeed) noexcept;
  [[nodiscard]] float GetMinSpeed() const noexcept { return m_minSpeed; }
  [[nodiscard]] float GetMaxSpeed() const noexcept { return m_maxSpeed; }

  void SetSizeRange(float startSize, float endSize) noexcept;
  [[nodiscard]] float GetStartSize() const noexcept { return m_startSize; }
  [[nodiscard]] float GetEndSize() const noexcept { return m_endSize; }

  void SetStartColor(float r, float g, float b, float a) noexcept;
  [[nodiscard]] std::array<float, 4> GetStartColor() const noexcept {
    return m_startColor;
  }

  void SetEndColor(float r, float g, float b, float a) noexcept;
  [[nodiscard]] std::array<float, 4> GetEndColor() const noexcept {
    return m_endColor;
  }

  void SetRotationSpeedRange(float minDegPerSec, float maxDegPerSec) noexcept;
  [[nodiscard]] float GetMinRotationSpeed() const noexcept {
    return m_minRotationSpeed;
  }
  [[nodiscard]] float GetMaxRotationSpeed() const noexcept {
    return m_maxRotationSpeed;
  }

  // =========================================================================
  // Physics
  // =========================================================================

  void SetGravityMultiplier(float multiplier) noexcept {
    m_gravityMultiplier = multiplier;
  }
  [[nodiscard]] float GetGravityMultiplier() const noexcept {
    return m_gravityMultiplier;
  }

  void SetVelocityDamping(float damping) noexcept {
    m_velocityDamping = damping;
  }
  [[nodiscard]] float GetVelocityDamping() const noexcept {
    return m_velocityDamping;
  }

  // =========================================================================
  // Rendering
  // =========================================================================

  void SetBlendMode(ParticleBlendMode mode) noexcept { m_blendMode = mode; }
  [[nodiscard]] ParticleBlendMode GetBlendMode() const noexcept {
    return m_blendMode;
  }

  void SetTextureAssetId(const std::string &assetId) {
    m_textureAssetId = assetId;
  }
  [[nodiscard]] const std::string &GetTextureAssetId() const noexcept {
    return m_textureAssetId;
  }

  // =========================================================================
  // Playback Control
  // =========================================================================

  void Play();
  void Stop();
  void Pause();
  void Resume();
  void Restart();

  [[nodiscard]] bool IsPlaying() const noexcept {
    return m_playing && !m_paused;
  }
  [[nodiscard]] bool IsPaused() const noexcept { return m_paused; }

  void SetPlayOnAwake(bool playOnAwake) noexcept {
    m_playOnAwake = playOnAwake;
  }
  [[nodiscard]] bool GetPlayOnAwake() const noexcept { return m_playOnAwake; }

  // =========================================================================
  // Burst Emission
  // =========================================================================

  void Burst(uint32_t count);

  // =========================================================================
  // AI Integration
  // =========================================================================

  /// @brief Set a natural-language hint for AI to understand this effect
  /// @param hint Description like "campfire", "magic sparkles", "rain"
  void SetPromptHint(const std::string &hint) { m_promptHint = hint; }
  [[nodiscard]] const std::string &GetPromptHint() const noexcept {
    return m_promptHint;
  }

  /// @brief Load a named preset (can be called by AI copilot)
  /// @param presetName Built-in presets: "fire", "smoke", "sparks", "rain",
  /// "snow", "magic"
  void SetPreset(const std::string &presetName);

  /// @brief Set a callback for AI-driven per-particle behavior updates
  /// The callback receives (particle, deltaTime) and can modify velocity/color
  using ParticleBehaviorCallback = std::function<void(Particle &, float)>;
  void SetBehaviorCallback(ParticleBehaviorCallback callback) {
    m_behaviorCallback = std::move(callback);
  }

  // =========================================================================
  // Data Access (for rendering)
  // =========================================================================

  [[nodiscard]] const std::vector<Particle> &GetParticles() const noexcept {
    return m_particles;
  }
  [[nodiscard]] uint32_t GetActiveParticleCount() const noexcept {
    return m_activeCount;
  }

protected:
  void OnBeginPlay() override;
  void OnEndPlay() override;
  void OnUpdate(float deltaTime) override;

private:
  void SpawnParticle();
  void UpdateParticles(float deltaTime);
  void InitializeParticle(Particle &p);
  [[nodiscard]] std::array<float, 3> GetEmissionDirection() const;
  [[nodiscard]] std::array<float, 3> GetEmissionOffset() const;

  // Emission settings
  float m_emissionRate{10.0f};
  float m_duration{5.0f};
  bool m_looping{true};
  uint32_t m_maxParticles{1000};
  ParticleEmissionShape m_emissionShape{ParticleEmissionShape::Point};
  float m_shapeRadius{1.0f};
  float m_shapeAngle{25.0f};
  std::array<float, 3> m_shapeExtents{1.0f, 1.0f, 1.0f};

  // Particle properties
  float m_minLifetime{1.0f};
  float m_maxLifetime{2.0f};
  float m_minSpeed{1.0f};
  float m_maxSpeed{3.0f};
  float m_startSize{0.5f};
  float m_endSize{0.1f};
  std::array<float, 4> m_startColor{1.0f, 1.0f, 1.0f, 1.0f};
  std::array<float, 4> m_endColor{1.0f, 1.0f, 1.0f, 0.0f};
  float m_minRotationSpeed{0.0f};
  float m_maxRotationSpeed{0.0f};

  // Physics
  float m_gravityMultiplier{0.0f};
  float m_velocityDamping{0.0f};

  // Rendering
  ParticleBlendMode m_blendMode{ParticleBlendMode::Alpha};
  std::string m_textureAssetId;

  // Playback state
  bool m_playing{false};
  bool m_paused{false};
  bool m_playOnAwake{true};
  float m_elapsedTime{0.0f};
  float m_emissionAccumulator{0.0f};

  // Particle pool
  std::vector<Particle> m_particles;
  uint32_t m_activeCount{0};

  // AI integration
  std::string m_promptHint;
  ParticleBehaviorCallback m_behaviorCallback;

  // Random number generation (mutable for const methods that need randomness)
  mutable std::mt19937 m_rng;
  mutable std::uniform_real_distribution<float> m_dist01{0.0f, 1.0f};
};

} // namespace Aetherion::Scene
