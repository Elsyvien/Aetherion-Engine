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

## Copilot Plan Format (Engine Contract)

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

## Tool-Calling Friendly Contract

For real agentic workflows, treat the plan format above as the **tool call
payload**. Instead of asking the model for raw JSON, register a single tool
that returns the plan object. This lets Claude, ChatGPT, Gemini, or any other
provider stay inside their native tool-calling mode while keeping Aetherion's
runtime deterministic.

**Recommended tool name:** `aetherion_plan`

**Tool input schema:** see [`docs/schemas/copilot_plan.schema.json`](schemas/copilot_plan.schema.json).

**LLM output expectation:** a single tool call with the plan JSON as arguments.

### Provider adapters (conceptual)

The adapter layer only has two jobs:

1. Provide the model with **scene context** and the **tool schema**.
2. Translate the tool call output into the plan JSON for the editor.

Example scene context payload to inject into the system prompt:

```json
{
  "selected_entity": { "id": 12, "name": "PlayerStart" },
  "entity_count": 64,
  "camera": { "mode": "editor", "position": [0, 3, 8] }
}
```

### OpenAI / ChatGPT (tools)

Send the schema as a tool and require a tool call:

```json
{
  "model": "gpt-4.1",
  "messages": [
    { "role": "system", "content": "You are an editor copilot. Use the tool." },
    { "role": "user", "content": "Spawn 3 lights in a row." }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "aetherion_plan",
        "description": "Return an editor plan for Aetherion.",
        "parameters": { "$ref": "docs/schemas/copilot_plan.schema.json" }
      }
    }
  ],
  "tool_choice": { "type": "function", "function": { "name": "aetherion_plan" } }
}
```

### Anthropic / Claude (tool use)

Register the same schema as a tool and instruct Claude to use it:

```json
{
  "model": "claude-3-7-sonnet",
  "system": "Use the aetherion_plan tool and return tool use only.",
  "tools": [
    {
      "name": "aetherion_plan",
      "description": "Return an editor plan for Aetherion.",
      "input_schema": { "$ref": "docs/schemas/copilot_plan.schema.json" }
    }
  ],
  "messages": [{ "role": "user", "content": "Make a 2x2 grid of cubes." }]
}
```

### Google / Gemini (function calling)

Gemini's function calling can register the same plan schema:

```json
{
  "model": "gemini-1.5-pro",
  "contents": [{ "role": "user", "parts": [{ "text": "Frame the selected entity." }] }],
  "tools": [{
    "functionDeclarations": [{
      "name": "aetherion_plan",
      "description": "Return an editor plan for Aetherion.",
      "parameters": { "$ref": "docs/schemas/copilot_plan.schema.json" }
    }]
  }]
}
```

> **Note:** The adapter should only accept tool output that validates against
> the schema. Reject freeform text responses to keep the workflow deterministic.

## Suggested LLM Prompt Template (copy/paste mode)

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

## Implementation Checklist (when wiring a provider)

1. **Fetch scene context** (selected entity, entity counts, camera pose).
2. **Send tool schema** (`aetherion_plan`) to the provider.
3. **Force tool calling** and reject non-tool responses.
4. **Validate JSON** against the schema file.
5. **Dispatch plan steps** through `AICopilotProcessor`.

This keeps the runtime editor logic identical while enabling external
LLMs to drive a safe, deterministic tool workflow.
