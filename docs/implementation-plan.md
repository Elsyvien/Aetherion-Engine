# Implementation Plan (AI/Editor Iteration)

This pass implements the next roadmap slice:
- Add editor UX for AIBehaviorComponent and virtual assets.
- Broaden copilot intents beyond spawning.
- Scaffold semantic scripting runtime hooks and runtime AI execution.
- Extend virtual asset pipeline with queued placeholders.
- Add validation hooks.

Execution steps:
1) Editor Inspector: render AIBehaviorComponent fields (prompt asset picker, inline prompt, personality, state/debug) plus “Add Component” entry; mark virtual assets in the asset browser list.
2) Copilot: accept transform/delete/duplicate/selection-aware intents, basic dry-run preview, and keep undo/redo via command history.
3) Scripting runtime: expose error diagnostics, inline generation outputs, and a hook point for Python/pybind integration; surface the generated path in runtime.
4) AI runtime: stub policy interface in AIBehaviorComponent with per-frame budget and context/state reporting.
5) Virtual assets: track virtual asset entries, queue generation placeholders, and surface status text in the browser; keep cooking-ready IDs.
6) Notes/validation: document the behavior changes and suggest follow-up tests/build.
