# Agentic Copilot Workflow

This document lays the groundwork for an agentic workflow that can be driven by
external LLMs (Claude, ChatGPT, Gemini, etc.) while still running safely inside
Aetherion's editor.

## Goals

- **LLM-agnostic**: Any provider can be used if it can emit the JSON plan format
  described below.
- **Deterministic tools**: The editor only executes known tool actions.
- **Copy/paste ready**: Until direct API integration is shipped, an LLM response
  can be pasted into the AI Copilot panel to execute a plan.

## Copilot Plan Format

The AI Copilot accepts JSON plans. The root object must contain a `steps` array.
Each step references a supported tool and provides arguments for execution.

```json
{
  "summary": "Spawn a 3x3 grid of cubes and move the selection up 2",
  "steps": [
    {
      "tool": "spawn_entity",
      "args": {
        "type": "cube",
        "count": 9,
        "grid": true,
        "spacing": 2.5,
        "origin": {"x": 0, "y": 0, "z": 0}
      }
    },
    {
      "tool": "move_selection",
      "args": {
        "direction": "up",
        "distance": 2
      }
    }
  ]
}
```

### Supported tools

| Tool | Purpose | Arguments |
| --- | --- | --- |
| `spawn_entity` | Create entities in the scene | `type` (`entity`, `cube`, `light`, `camera`), `count` (int), `grid` (bool), `spacing` (float), `origin` (`{x,y,z}`) |
| `move_selection` | Move the currently selected entity | `direction` (`up`, `down`, `left`, `right`, `forward`, `back`), `distance` (float) **or** `offset` (`{x,y,z}`) |
| `delete_selection` | Delete the selected entity | No args |
| `duplicate_selection` | Duplicate the selected entity | No args |
| `parent_selection` | Reparent the selected entity | `target_id` (int) or `"root"` |
| `focus_selection` | Frame the selected entity in the viewport | No args |

## Suggested LLM Prompt Template

When you connect an LLM, provide it with:

- The tool list above (or a JSON schema variant).
- A short scene context (selected entity name/id, counts).
- A constraint to only respond with a JSON plan.

Example prompt snippet:

```
You are an editor copilot. Emit ONLY valid JSON.
Tools: spawn_entity, move_selection, delete_selection, duplicate_selection,
parent_selection, focus_selection.
```

## Provider Setup (future)

Aetherion does not yet ship direct LLM API calls, but the plan format is ready
for integration. When adding a provider, wire it to emit the JSON plan structure
above so the editor can execute it without custom parsing logic.

Recommended environment variable names:

- `OPENAI_API_KEY`
- `ANTHROPIC_API_KEY`
- `GOOGLE_API_KEY`

