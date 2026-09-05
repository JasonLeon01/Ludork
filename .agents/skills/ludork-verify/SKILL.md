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
| First-party C++ | Format changed headers/sources, then `sh tools/build_cpp.sh Sample Debug` or the existing configured build's affected target. |
| C++/Lua host and implementation boundaries | `.tools/ScriptTools/ScriptTools impl-boundary-check Sample` |
| Core bindings or bindgen | Rebuild ScriptTools if changed, then the native build to regenerate/compile bindings; inspect affected stubs/metadata and exercise the changed Lua boundary. |
| Lua, handwritten metadata or stubs | EmmyLua formatting and full-workspace diagnostics for `Sample`, then a focused runtime/editor check when behaviour changed. |
| Declarative UI JSON or asset moves | `.tools/ScriptTools/ScriptTools ui-assets validate Sample`; inspect the affected UI and its references. |
| Native UI registry/adapters or editor UI registration | `.tools/ScriptTools/ScriptTools ui-adapter-check .`; build the affected editor/runtime/preview host. |
| Shell/build/CI wiring | Syntax-check changed `.sh` with `sh -n`; validate workflow YAML with `actionlint` when available; exercise the affected local entry point or report platform limitations. |
| Packaging/templates | Run the affected platform/variant entry point from `tools/README.md` and inspect its package. Check that templates retain stubs and game packages exclude them when Lua packaging changes. |

Useful targeted entry points are `sh tools/build_script_tools.sh`, `sh tools/build_ui_preview_host.sh Debug`, `sh tools/run_editor.sh` and `sh tools/run_cpp.sh Sample Debug`. The C++ build already validates UI assets; avoid repeating the same successful check without an intervening relevant change. Shared `.tools`, `Sample/build`, `bin`, `obj` and packaging outputs must not be mutated concurrently by multiple builds or agents.

## EmmyLua

Use the installed EmmyLua editor integration or language server, with `Sample/.emmyrc.json` and `Sample` as the workspace. Format every changed `.lua`/`.d.lua` using the built-in formatter or `textDocument/formatting`, apply those edits, then run full-workspace diagnostics. Include errors, warnings and hints; keep all three at zero whenever reasonably possible.

Preserve authoritative native/schema types. Fix control flow and callers; a narrow suppression is appropriate only for a confirmed analyser limitation. If the server, generated stubs or dependencies are unavailable, identify the missing prerequisite, complete independent checks and report diagnostics as unrun. Do not substitute a different formatter or claim a syntax check is full EmmyLua validation.

## CI and completion

[Export Editor](../../../.github/workflows/export-editor.yml) prepares each platform, builds independent native components in a matrix, then packages Windows and macOS artifacts. Keep that dependency order and artifact handoff intact. Superseded runs of the same PR may be cancelled; main-branch and manual export runs use distinct concurrency groups. Documentation ships in the editor, so documentation changes still need package coverage.

For failures, fix the cause and rerun affected checks. Report the checks actually run, their results, and any unverified behaviour; follow AGENTS.md for final review and cleanup.
