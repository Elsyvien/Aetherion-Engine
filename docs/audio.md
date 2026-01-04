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

---

## MVP Audio Roadmap

This section outlines the concrete steps to bring audio from minimal one-shot
playback to a production-ready subsystem.

### Current Limitations
- Fire-and-forget one-shot playback only.
- No looping, pitch, or volume control after start.
- No 3D spatialization.
- No streaming (all audio loaded into memory).
- No audio graph or mixer.

### Milestone 1: Streaming Playback
- [ ] Use miniaudio's `ma_decoder` for file streaming.
- [ ] Support long audio files (music, ambience) without loading fully.
- [ ] Add `AudioSourceComponent.streaming` flag.
- [ ] Limit memory usage for large clips.

### Milestone 2: Playback Controls
- [ ] Pause / Resume / Stop per sound.
- [ ] Runtime volume and pitch adjustment.
- [ ] Looping with optional loop count.
- [ ] Fade in / fade out helpers.

### Milestone 3: 3D Spatialization
- [ ] Bind `AudioListenerComponent` to camera/player position.
- [ ] Set source position from `TransformComponent`.
- [ ] Use miniaudio's spatialization or custom panning.
- [ ] Attenuation model (linear, inverse, exponential).
- [ ] Doppler effect (optional).

### Milestone 4: Mixer / Audio Graph
- [ ] Introduce mixer buses (master, SFX, music, voice).
- [ ] Per-bus volume and mute controls.
- [ ] Effect chain per bus (reverb, EQ, compressor).
- [ ] Visual audio graph editor (future).

### Milestone 5: Editor Integration
- [ ] Audio clip preview in Asset Browser.
- [ ] Waveform visualization.
- [ ] AudioSource gizmo in scene (speaker icon + attenuation sphere).
- [ ] Play/stop controls in Inspector.
- [ ] Volume slider and playback position scrubber.

### Milestone 6: Runtime Optimization
- [ ] Voice limiting / priority system.
- [ ] Occlusion / obstruction queries (raycast-based).
- [ ] Async loading and decoding.
- [ ] Profiling hooks for audio thread timing.

### Implementation Notes
- miniaudio is already integrated; extend rather than replace.
- Consider FMOD/Wwise integration as optional high-end path.
- Audio thread must be lock-free for real-time safety.
