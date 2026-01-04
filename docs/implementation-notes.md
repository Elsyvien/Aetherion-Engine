# Implementation Notes (HUD, Copilot, Tests)

- Viewport HUD now surfaces AI behavior state/reason for the selected entity.
- Copilot dry-run previews prompt a confirmation dialog before applying changes.
- Virtual assets are labeled in the asset browser for visibility.
- Self-tests added: `AetherionSelfTests` covers virtual asset registration/caching and scripting hot-reload regeneration.
- AI inspector UI exposes execution mode, prompt asset/inline prompt, personality/knowledge/context, and decision interval.
- Added optional embedded Python bridge for semantic scripting (`AETHERION_ENABLE_PYTHON`), with console error sink.
- View menu toggle for AI HUD plus a short history trail of recent AI states.
- Copilot intent harness (`AetherionCopilotTest`) exercises dry-run move and parenting commands.
