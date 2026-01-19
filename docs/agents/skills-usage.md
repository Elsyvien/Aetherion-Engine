# Skill Usage Rules

## Definition
- A skill is a set of local instructions stored in a `SKILL.md` file.

## Trigger rules
- If the user names a skill (with `$SkillName` or plain text) or the task clearly matches a skill's description in the catalog, you must use that skill for that turn.
- Do not carry skills across turns unless re-mentioned.

## Missing or blocked
- If a named skill is not in the catalog or the path cannot be read, say so briefly and continue with the best fallback.

## Progressive disclosure
1. After deciding to use a skill, open its `SKILL.md`. Read only enough to follow the workflow.
2. If `SKILL.md` points to extra folders such as `references/`, load only the specific files needed for the request; do not bulk-load everything.
3. If `scripts/` exist, prefer running or patching them instead of retyping large code blocks.
4. If `assets/` or templates exist, reuse them instead of recreating from scratch.

## Coordination and sequencing
- If the user explicitly names multiple skills, use all named skills. If multiple skills match by description, choose the minimal set that covers the request and state the order you will use them.
- Announce which skill(s) you are using and why (one short line). If you skip an obvious skill, say why.

## Context hygiene
- Keep context small: summarize long sections instead of pasting them; only load extra files when needed.
- Avoid deep reference-chasing: prefer opening only files directly linked from `SKILL.md` unless you are blocked.
- When variants exist (frameworks, providers, domains), pick only the relevant reference file(s) and note that choice.

## Safety and fallback
- If a skill cannot be applied cleanly (missing files, unclear instructions), state the issue, pick the next-best approach, and continue.
