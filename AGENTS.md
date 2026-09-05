# Ludork

Ludork is a 2D RPG engine with an Avalonia/.NET 9 editor, a native C++ runtime and Lua gameplay.

## Working agreement

- Complete the requested work through implementation and relevant verification. Resolve routine, reversible choices from the task and repository; ask only when missing information materially changes the outcome or an action needs authorization. Continue independent work while waiting.
- Preserve the user's staged and unstaged changes. Do not stage, unstage, commit, reset, discard or publish changes without explicit authorization. Leave your changes unstaged for review.
- Prefer the simplest design that satisfies the settled contract. Replace superseded APIs, paths and data shapes directly; do not add compatibility reads, migrations, fallbacks or aliases unless the task or documented contract explicitly requires them.
- Edit first-party sources and build wiring. Do not hand-edit ignored third-party code, generated files or build/session artifacts, or add patches/overlays for them. Regenerate outputs through their owning tools. Do not force-add ignored paths.
- Remove test code and scaffolding created for the task after verification. Do not delete existing tests, Sample assets or other people's artifacts. Keep temporary probes outside the repository when practical.
- Report the result, relevant verification and any remaining limitation concisely in the user's language. If a repository instruction or skill blocks progress, identify the exact file and rule rather than requesting unexplained confirmation.

## Repository map and task context

| Area | Source of truth |
|---|---|
| Editor | `Views/`, `ViewModels/`, `Models/`, `Controls/`, `Utils/`, `Ludork.csproj` |
| Native runtime and bindings | `Sample/Engine/Runtime/`, `Sample/Engine/Standard/`, `Sample/Engine/Source/Core/` |
| Lua implementation, public declarations, editor metadata | `Sample/Scripts/**/*.lua`, mirrored `Sample/Scripts/stub/**/*.d.lua`, sibling `*_meta.lua` |
| Game data and UI | `Sample/Data/`; declarative assets under `Sample/Data/UI/Assets/` |
| Generators and build/pack entry points | `ScriptTools/`, [tools/README.md](tools/README.md) |
| Architecture and runtime behaviour | [English docs](docs/en_GB/00.Ludork%20Documentation.md); other locale trees under `docs/` |

Before changing behaviour, read the matching English documentation. Load only the relevant pages and repository skills below, including when working from the repository root. Skills supply task-specific procedures; current user instructions take precedence over their guidance, subject to system and developer instructions.

| Task | Read before editing |
|---|---|
| Lua, Standard globals, classes, Script Mixins or Sample gameplay | [ludork-lua](.agents/skills/ludork-lua/SKILL.md) |
| Core bindings, bindgen, Blueprint metadata or execution | [ludork-bindings](.agents/skills/ludork-bindings/SKILL.md) |
| Avalonia form inputs, declarative UI assets/controllers or native UI adapters | [ludork-ui](.agents/skills/ludork-ui/SKILL.md) |
| Choosing/running checks, build tools or CI changes | [ludork-verify](.agents/skills/ludork-verify/SKILL.md) |

## Editing conventions

- Editor C#: no comments/docstrings or unnecessary `try/catch`; use explicit types unless the full type name is excessively long.
- First-party C++: run `clang-format -i` on each changed source/header using the repository `.clang-format`. Exclude third-party and generated code.
- Lua: format each changed `.lua`/`.d.lua` with EmmyLua and run full-workspace EmmyLua diagnostics. Keep authoritative types exact; fix control flow or callers instead of widening contracts or adding broad ignores.
- When behaviour, APIs, paths or conventions described in docs change, update every locale in the same task, including `en_GB` and `zh_CN`. Sample changes also require checking the relevant Sample Gameplay and API pages. Re-read edited pages, remove repetition, and document settled behaviour rather than implementation history or agent instructions.

## Execution and completion

Batch independent searches and reads. When delegation is available and permitted, use subagents for bounded independent investigation, implementation or review that benefits the task; assign clear ownership and integrate their results. Keep overlapping edits and shared build outputs sequential. Small tasks do not need delegation or a formal plan.

Choose verification from the affected behaviour and the [validation workflow](.agents/skills/ludork-verify/SKILL.md). Complete required checks; do not add tests that merely mirror a trivial edit. Broaden or repeat checks only for new changes, failures or unresolved risks. Before finishing, review your diff for correctness, scope, stale references and leftover test scaffolding, and state any check that could not run.
