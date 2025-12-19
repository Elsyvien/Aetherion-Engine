#pragma once

namespace Aetherion::Audio
{
class AudioEngineStub
{
public:
    AudioEngineStub() = default;
    ~AudioEngineStub() = default;

    void Initialize() {}
    void Shutdown() {}
};

// ===============================
// TODO: Audio System Placeholder
// ===============================
// This module will encapsulate audio device management, mixing, and spatialization.
// No implementation exists at this stage.

inline void TouchAudioModule() {}
} // namespace Aetherion::Audio
