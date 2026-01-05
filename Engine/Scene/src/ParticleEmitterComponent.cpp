#include "Aetherion/Scene/ParticleEmitterComponent.h"

#include <algorithm>
#include <chrono>
#include <cmath>


namespace Aetherion::Scene {

namespace {
constexpr float kGravity = -9.81f;
constexpr float kDegToRad = 3.14159265359f / 180.0f;

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

void LerpColor(float out[4], const float a[4], const float b[4], float t) {
  for (int i = 0; i < 4; ++i) {
    out[i] = Lerp(a[i], b[i], t);
  }
}
} // namespace

ParticleEmitterComponent::ParticleEmitterComponent() {
  // Seed RNG with current time
  auto seed = static_cast<uint32_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  m_rng.seed(seed);

  // Pre-allocate particle pool
  m_particles.resize(m_maxParticles);
}

void ParticleEmitterComponent::SetMaxParticles(uint32_t max) noexcept {
  m_maxParticles = max;
  m_particles.resize(max);
  m_activeCount = std::min(m_activeCount, max);
}

void ParticleEmitterComponent::SetShapeExtents(float x, float y,
                                               float z) noexcept {
  m_shapeExtents = {x, y, z};
}

void ParticleEmitterComponent::SetLifetimeRange(float minSec,
                                                float maxSec) noexcept {
  m_minLifetime = minSec;
  m_maxLifetime = maxSec;
}

void ParticleEmitterComponent::SetSpeedRange(float minSpeed,
                                             float maxSpeed) noexcept {
  m_minSpeed = minSpeed;
  m_maxSpeed = maxSpeed;
}

void ParticleEmitterComponent::SetSizeRange(float startSize,
                                            float endSize) noexcept {
  m_startSize = startSize;
  m_endSize = endSize;
}

void ParticleEmitterComponent::SetStartColor(float r, float g, float b,
                                             float a) noexcept {
  m_startColor = {r, g, b, a};
}

void ParticleEmitterComponent::SetEndColor(float r, float g, float b,
                                           float a) noexcept {
  m_endColor = {r, g, b, a};
}

void ParticleEmitterComponent::SetRotationSpeedRange(
    float minDegPerSec, float maxDegPerSec) noexcept {
  m_minRotationSpeed = minDegPerSec;
  m_maxRotationSpeed = maxDegPerSec;
}

void ParticleEmitterComponent::Play() {
  m_playing = true;
  m_paused = false;
  m_elapsedTime = 0.0f;
  m_emissionAccumulator = 0.0f;
}

void ParticleEmitterComponent::Stop() {
  m_playing = false;
  m_paused = false;
  // Clear all particles
  for (auto &p : m_particles) {
    p.alive = false;
  }
  m_activeCount = 0;
}

void ParticleEmitterComponent::Pause() {
  if (m_playing) {
    m_paused = true;
  }
}

void ParticleEmitterComponent::Resume() { m_paused = false; }

void ParticleEmitterComponent::Restart() {
  Stop();
  Play();
}

void ParticleEmitterComponent::Burst(uint32_t count) {
  for (uint32_t i = 0; i < count && m_activeCount < m_maxParticles; ++i) {
    SpawnParticle();
  }
}

void ParticleEmitterComponent::SetPreset(const std::string &presetName) {
  if (presetName == "fire") {
    SetEmissionRate(50.0f);
    SetLifetimeRange(0.5f, 1.5f);
    SetSpeedRange(2.0f, 4.0f);
    SetSizeRange(0.3f, 0.0f);
    SetStartColor(1.0f, 0.6f, 0.1f, 1.0f);
    SetEndColor(1.0f, 0.1f, 0.0f, 0.0f);
    SetGravityMultiplier(-0.5f); // Rise up
    SetBlendMode(ParticleBlendMode::Additive);
    SetEmissionShape(ParticleEmissionShape::Cone);
    SetShapeAngle(15.0f);
    m_promptHint = "fire";
  } else if (presetName == "smoke") {
    SetEmissionRate(20.0f);
    SetLifetimeRange(2.0f, 4.0f);
    SetSpeedRange(0.5f, 1.5f);
    SetSizeRange(0.2f, 1.0f);
    SetStartColor(0.3f, 0.3f, 0.3f, 0.8f);
    SetEndColor(0.5f, 0.5f, 0.5f, 0.0f);
    SetGravityMultiplier(-0.2f);
    SetBlendMode(ParticleBlendMode::Alpha);
    SetEmissionShape(ParticleEmissionShape::Sphere);
    SetShapeRadius(0.3f);
    m_promptHint = "smoke";
  } else if (presetName == "sparks") {
    SetEmissionRate(30.0f);
    SetLifetimeRange(0.3f, 0.8f);
    SetSpeedRange(5.0f, 10.0f);
    SetSizeRange(0.1f, 0.02f);
    SetStartColor(1.0f, 0.9f, 0.5f, 1.0f);
    SetEndColor(1.0f, 0.3f, 0.0f, 0.0f);
    SetGravityMultiplier(1.0f);
    SetBlendMode(ParticleBlendMode::Additive);
    SetEmissionShape(ParticleEmissionShape::Sphere);
    SetShapeRadius(0.1f);
    m_promptHint = "sparks";
  } else if (presetName == "rain") {
    SetEmissionRate(200.0f);
    SetLifetimeRange(0.5f, 1.0f);
    SetSpeedRange(15.0f, 20.0f);
    SetSizeRange(0.02f, 0.02f);
    SetStartColor(0.6f, 0.7f, 0.9f, 0.6f);
    SetEndColor(0.6f, 0.7f, 0.9f, 0.3f);
    SetGravityMultiplier(2.0f);
    SetBlendMode(ParticleBlendMode::Alpha);
    SetEmissionShape(ParticleEmissionShape::Box);
    SetShapeExtents(10.0f, 0.1f, 10.0f);
    m_promptHint = "rain";
  } else if (presetName == "snow") {
    SetEmissionRate(100.0f);
    SetLifetimeRange(3.0f, 5.0f);
    SetSpeedRange(0.5f, 1.5f);
    SetSizeRange(0.05f, 0.05f);
    SetStartColor(1.0f, 1.0f, 1.0f, 0.9f);
    SetEndColor(1.0f, 1.0f, 1.0f, 0.5f);
    SetGravityMultiplier(0.1f);
    SetBlendMode(ParticleBlendMode::Alpha);
    SetEmissionShape(ParticleEmissionShape::Box);
    SetShapeExtents(10.0f, 0.1f, 10.0f);
    SetRotationSpeedRange(-90.0f, 90.0f);
    m_promptHint = "snow";
  } else if (presetName == "magic") {
    SetEmissionRate(40.0f);
    SetLifetimeRange(1.0f, 2.0f);
    SetSpeedRange(1.0f, 3.0f);
    SetSizeRange(0.2f, 0.0f);
    SetStartColor(0.5f, 0.2f, 1.0f, 1.0f);
    SetEndColor(0.2f, 0.8f, 1.0f, 0.0f);
    SetGravityMultiplier(0.0f);
    SetBlendMode(ParticleBlendMode::Additive);
    SetEmissionShape(ParticleEmissionShape::Sphere);
    SetShapeRadius(0.5f);
    SetRotationSpeedRange(-180.0f, 180.0f);
    m_promptHint = "magic";
  }
}

void ParticleEmitterComponent::OnBeginPlay() {
  if (m_playOnAwake) {
    Play();
  }
}

void ParticleEmitterComponent::OnEndPlay() { Stop(); }

void ParticleEmitterComponent::OnUpdate(float deltaTime) {
  if (!m_playing || m_paused) {
    return;
  }

  m_elapsedTime += deltaTime;

  // Check if duration exceeded (non-looping)
  bool emissionEnabled = true;
  if (!m_looping && m_elapsedTime > m_duration) {
    emissionEnabled = false;
    // Stop completely when all particles are dead
    if (m_activeCount == 0) {
      m_playing = false;
      return;
    }
  }

  // Spawn new particles based on emission rate
  if (emissionEnabled && m_emissionRate > 0.0f) {
    m_emissionAccumulator += m_emissionRate * deltaTime;
    while (m_emissionAccumulator >= 1.0f && m_activeCount < m_maxParticles) {
      SpawnParticle();
      m_emissionAccumulator -= 1.0f;
    }
  }

  // Update existing particles
  UpdateParticles(deltaTime);
}

void ParticleEmitterComponent::SpawnParticle() {
  // Find a dead particle slot
  for (auto &p : m_particles) {
    if (!p.alive) {
      InitializeParticle(p);
      ++m_activeCount;
      return;
    }
  }
}

void ParticleEmitterComponent::InitializeParticle(Particle &p) {
  p.alive = true;
  p.age = 0.0f;
  p.lifetime = Lerp(m_minLifetime, m_maxLifetime, m_dist01(m_rng));

  // Get emission position offset
  auto offset = GetEmissionOffset();
  p.position[0] = offset[0];
  p.position[1] = offset[1];
  p.position[2] = offset[2];

  // Get emission direction and speed
  auto dir = GetEmissionDirection();
  float speed = Lerp(m_minSpeed, m_maxSpeed, m_dist01(m_rng));
  p.velocity[0] = dir[0] * speed;
  p.velocity[1] = dir[1] * speed;
  p.velocity[2] = dir[2] * speed;

  // Size
  p.startSize = m_startSize;
  p.endSize = m_endSize;
  p.size = p.startSize;

  // Color
  for (int i = 0; i < 4; ++i) {
    p.startColor[i] = m_startColor[i];
    p.endColor[i] = m_endColor[i];
    p.color[i] = p.startColor[i];
  }

  // Rotation
  p.rotation = m_dist01(m_rng) * 360.0f;
  p.rotationSpeed =
      Lerp(m_minRotationSpeed, m_maxRotationSpeed, m_dist01(m_rng));
}

void ParticleEmitterComponent::UpdateParticles(float deltaTime) {
  uint32_t newActiveCount = 0;

  for (auto &p : m_particles) {
    if (!p.alive) {
      continue;
    }

    p.age += deltaTime;

    // Check if particle died
    if (p.age >= p.lifetime) {
      p.alive = false;
      continue;
    }

    float t = p.age / p.lifetime; // Normalized age [0, 1]

    // Apply AI behavior callback if set
    if (m_behaviorCallback) {
      m_behaviorCallback(p, deltaTime);
    }

    // Apply gravity
    p.velocity[1] += kGravity * m_gravityMultiplier * deltaTime;

    // Apply velocity damping
    if (m_velocityDamping > 0.0f) {
      float damping = 1.0f - m_velocityDamping * deltaTime;
      damping = std::max(0.0f, damping);
      p.velocity[0] *= damping;
      p.velocity[1] *= damping;
      p.velocity[2] *= damping;
    }

    // Update position
    p.position[0] += p.velocity[0] * deltaTime;
    p.position[1] += p.velocity[1] * deltaTime;
    p.position[2] += p.velocity[2] * deltaTime;

    // Update rotation
    p.rotation += p.rotationSpeed * deltaTime;

    // Interpolate size
    p.size = Lerp(p.startSize, p.endSize, t);

    // Interpolate color
    LerpColor(p.color, p.startColor, p.endColor, t);

    ++newActiveCount;
  }

  m_activeCount = newActiveCount;
}

std::array<float, 3> ParticleEmitterComponent::GetEmissionDirection() const {
  std::array<float, 3> dir = {0.0f, 1.0f, 0.0f}; // Default: up

  // For cone, add random angle variation
  if (m_emissionShape == ParticleEmissionShape::Cone) {
    float theta = m_dist01(m_rng) * 2.0f * 3.14159265359f;
    float phi = m_dist01(m_rng) * m_shapeAngle * kDegToRad;

    float sinPhi = std::sin(phi);
    dir[0] = sinPhi * std::cos(theta);
    dir[2] = sinPhi * std::sin(theta);
    dir[1] = std::cos(phi);
  } else if (m_emissionShape == ParticleEmissionShape::Sphere) {
    // Random direction on sphere
    float theta = m_dist01(m_rng) * 2.0f * 3.14159265359f;
    float phi = std::acos(2.0f * m_dist01(m_rng) - 1.0f);

    dir[0] = std::sin(phi) * std::cos(theta);
    dir[1] = std::sin(phi) * std::sin(theta);
    dir[2] = std::cos(phi);
  }
  // Point and Box use default up direction

  return dir;
}

std::array<float, 3> ParticleEmitterComponent::GetEmissionOffset() const {
  std::array<float, 3> offset = {0.0f, 0.0f, 0.0f};

  switch (m_emissionShape) {
  case ParticleEmissionShape::Point:
    // No offset
    break;
  case ParticleEmissionShape::Sphere: {
    float r = m_shapeRadius * std::cbrt(m_dist01(m_rng));
    float theta = m_dist01(m_rng) * 2.0f * 3.14159265359f;
    float phi = std::acos(2.0f * m_dist01(m_rng) - 1.0f);

    offset[0] = r * std::sin(phi) * std::cos(theta);
    offset[1] = r * std::sin(phi) * std::sin(theta);
    offset[2] = r * std::cos(phi);
    break;
  }
  case ParticleEmissionShape::Box: {
    offset[0] = (m_dist01(m_rng) - 0.5f) * m_shapeExtents[0];
    offset[1] = (m_dist01(m_rng) - 0.5f) * m_shapeExtents[1];
    offset[2] = (m_dist01(m_rng) - 0.5f) * m_shapeExtents[2];
    break;
  }
  case ParticleEmissionShape::Cone:
    // Cone emits from a point, no offset
    break;
  }

  return offset;
}

} // namespace Aetherion::Scene
