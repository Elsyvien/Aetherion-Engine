# Audio

## AudioEngine
Files:
- `Engine/Audio/include/Aetherion/Audio/AudioEngine.h`
- `Engine/Audio/src/AudioEngine.cpp`

- Wraps miniaudio `ma_engine`.
- Initialize() / Shutdown() create and destroy the engine.
- PlayOneShot(path, volume) plays fire-and-forget sounds.

## AudioSystem
Files:
- `Engine/Audio/include/Aetherion/Audio/AudioSystem.h`
- `Engine/Audio/src/AudioSystem.cpp`

- Binds to a Scene and scans for AudioSourceComponent.
- Implements PlayOnAwake and Play requests.
- Uses AudioEngine::PlayOneShot (loop/pitch/spatial are not yet applied).

## Components
See `docs/scene.md`:
- AudioSourceComponent
- AudioListenerComponent
