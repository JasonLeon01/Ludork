---
name: ludork-verify
description: Select and run Ludork verification for a repository change, or maintain its build and CI workflows. Use for C#, C++, Lua/EmmyLua, bindgen, UI assets and packaging checks; scale checks to the affected behaviour.
---

# Ludork verification workflow

## Select checks from the change

Identify the task's changes, including new files, separately from unrelated pre-existing changes. Use the smallest set of checks that covers the requested outcome and affected contract, including the mandatory formatters and Lua diagnostics in AGENTS.md. Combine rows for cross-layer changes; these are selection criteria, not an instruction to run every row.

Commands below run from the repository root on macOS. Use matching `.bat` entry points and `.tools\ScriptTools\ScriptTools.exe` on Windows. [tools/README.md](../../../tools/README.md) owns build and packaging usage. Reuse an initialized environment; run `init` only when setup is missing, and rebuild ScriptTools after changing its sources before consuming its executable.

| Change | Relevant verification |
|---|---|
| Markdown or agent instructions only | Re-read changed content; check local links, paths, conflicting rules and locale parity. Validate skill frontmatter when changing skills. No editor/native build is needed. |
| Editor C# | `dotnet build Ludork.csproj -c Debug`; inspect affected UI behaviour for visible/input changes. |
| First-party C++ | Format changed headers/sources, then `sh tools/build_cpp.sh Game Debug` or the existing configured build's affected target. |
| C++/Lua host and implementation boundaries | `.tools/ScriptTools/ScriptTools impl-boundary-check Game` |
| Core bindings or bindgen | Rebuild ScriptTools if changed, then the native build to regenerate/compile bindings; inspect affected stubs/metadata and exercise the changed Lua boundary. |
| Lua, handwritten metadata or stubs | EmmyLua formatting and full-workspace diagnostics for `Game`, then a focused runtime/editor check when behaviour changed. |
| Declarative UI JSON or asset moves | `.tools/ScriptTools/ScriptTools ui-assets validate Game`; inspect the affected UI and its references. |
| Native UI registry/adapters or editor UI registration | `.tools/ScriptTools/ScriptTools ui-adapter-check .`; build the affected editor/runtime/preview host. |
| Shell/build/CI wiring | Syntax-check changed `.sh` with `sh -n`; validate workflow YAML with `actionlint` when available; exercise the affected local entry point or report platform limitations. |
| Packaging/templates | Run the affected platform/variant entry point from `tools/README.md` and inspect its package. Check that templates exclude UI/Locale Export outputs and records, retain all other stubs including General Data and native binding declarations, and game packages exclude the entire stub tree when Lua packaging changes. |

Useful targeted entry points are `sh tools/build_script_tools.sh`, `sh tools/build_ui_preview_host.sh Game Debug`, `sh tools/run_editor.sh` and `sh tools/run_cpp.sh Game Debug`. The C++ build already validates UI assets; avoid repeating the same successful check without an intervening relevant change. Shared `.tools`, `Game/build`, `bin`, `obj` and packaging outputs must not be mutated concurrently by multiple builds or agents.

## EmmyLua

Use the installed EmmyLua editor integration or language server, with `Game/.emmyrc.json` and `Game` as the workspace. Format every changed `.lua`/`.d.lua` using the built-in formatter or `textDocument/formatting`, apply those edits, then run full-workspace diagnostics. Include errors, warnings and hints; keep all three at zero whenever reasonably possible.

Preserve authoritative native/schema types. Fix control flow and callers; a narrow suppression is appropriate only for a confirmed analyser limitation. If the server, generated stubs or dependencies are unavailable, identify the missing prerequisite, complete independent checks and report diagnostics as unrun. Do not substitute a different formatter or claim a syntax check is full EmmyLua validation.

## CI and completion

PR validation selects additive checks from the actual changed paths: Engine/native inputs build Windows x64 and macOS ARM64 Release templates, plain and FFmpeg; Editor inputs compile Avalonia and referenced projects on both platforms without packaging; Scripts/EmmyLua inputs run pinned EmmyLua diagnostics for the complete Game workspace with `--warnings-as-errors`. Lua checking first reuses or builds Windows plain templates for matching native stubs and generates the remaining workspace declarations. Preserve shared preparation and artifact handoffs. New commits cancel older validation runs of the same PR; independent checks continue after another check fails.

The always-running `PR Validation` aggregate requires success from every selected check, rejecting failure, cancellation and unexpected skipping. Unselected checks may skip; documentation-only PRs need no build. Require `PR Validation` from GitHub Actions in the existing `main` protection rule or ruleset without removing other protections. Report this administrative setting separately from workflow implementation when permissions prevent applying it.

[Export Editor](../../../.github/workflows/export-editor.yml) packages at 08:00 and 16:00 UTC+8 and on manual dispatch, retaining artifact names and seven-day retention. Scheduled and manual temporary packages share serialization; only scheduled runs skip an unchanged SHA against the latest successful dual-platform default-branch temporary package. Successful manual default-branch runs update that baseline; other refs, tag packages and unsuccessful or skipped runs do not. Preserve the exact input-based component caches without prefix fallback. `v*` tag pushes build Windows ZIP and macOS DMG assets and create or update a draft Release with generated notes; never publish it automatically or overwrite a published release. Only the upload job needs `contents: write`. See [tools/README.md](../../../tools/README.md#pull-requests-and-automated-packages) for the operational contract.

For CI changes, check path combinations, deletion/rename detection, aggregate failure and skip handling, schedule deduplication, manual reruns and draft-only release handling. Local validation does not establish hosted Actions success or branch-protection enforcement.

For failures, fix the cause and rerun affected checks. Report the checks actually run, their results, and any unverified behaviour; follow AGENTS.md for final review and cleanup.
