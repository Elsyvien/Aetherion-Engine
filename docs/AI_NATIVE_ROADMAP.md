# Aetherion AI-Native Roadmap

This document outlines the strategy to transform Aetherion-Engine into an AI-Native Game Engine.

## Core Philosophy
In an AI-native engine, the "Prompt" is a first-class citizen, equal to Meshes or Textures. Users shouldn't just write code; they should declare intent.

## Phase 1: The Editor Copilot (The Hands)
**Goal:** Allow users to control the editor using Natural Language.
*   **Implementation:**
    *   Create a `AICopilotPanel` (Qt Dock Widget).
    *   Expose the internal `Command` system (found in `Editor/Commands`) to this panel.
    *   **Workflow:** User types "Spawn a grid of 10 cubes," and the Copilot generates and executes the necessary `CreateEntity` and `Transform` commands.

## Phase 2: Semantic Scripting (The Brain)
**Goal:** Replace the `ScriptingPlaceholder` with an LLM-driven Python environment.
*   **Implementation:**
    *   Embed Python (via `pybind11`).
    *   Instead of writing raw code, users provide a "Behavior Prompt" (e.g., "Chase the player if close").
    *   The engine generates the Python script in the background and attaches it.
    *   **Hot-reloading:** If the behavior is wrong, the user updates the prompt, and the code is regenerated instantly.

## Phase 3: Runtime AI Components (The Soul)
**Goal:** NPCs and Logic that "think" at runtime.
*   **Implementation:**
    *   Create `AIBehaviorComponent` in the ECS.
    *   **Fields:** `Personality`, `KnowledgeBase`, `CurrentContext`.
    *   At runtime, this component queries a lightweight local LLM (e.g., via ONNX or llama.cpp) to decide state transitions (Idle -> Attack) based on scene data.

## Phase 4: Generative Asset Pipeline
**Goal:** Generate assets on demand.
*   **Implementation:**
    *   Extend `AssetRegistry` to handle "Virtual Assets."
    *   Requesting `texture://generate/brick_wall` triggers a stable-diffusion generation task.
    *   Assets are cached to disk but created just-in-time.

## Immediate Next Steps
1.  Scaffold the `AICopilotPanel` in the Editor.
2.  Connect the panel to a mock "Command Processor" to prove it can spawn entities.
