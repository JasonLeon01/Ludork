---
name: ludork-bindings
description: Change or review Ludork Core bindings, bindgen, Blueprint metadata, editor metadata consumers or graph execution. Use for BIND annotations, metadata schemas and generated Lua surfaces.
---

# Ludork bindings and Blueprint metadata

## Select the owning contract

- For metadata and its editor consumers, read [Metadata Schema and Decorators](<../../../docs/en_GB/03.Lua and Blueprint Scripting/04.Blueprint Scripting/02.Metadata Schema and Decorators.md>).
- For graph execution, read [Execution Flow, Events and Variables](<../../../docs/en_GB/03.Lua and Blueprint Scripting/04.Blueprint Scripting/01.Execution Flow Events and Variables.md>).
- For native bindings or bindgen, read [Macro Reference](<../../../docs/en_GB/04.Native C++ Development/04.Macro Reference.md>) and [Generated Metadata and Stubs](<../../../docs/en_GB/04.Native C++ Development/05.Generated Metadata and Stubs.md>), then the relevant [class/property](<../../../docs/en_GB/04.Native C++ Development/02.Binding a Class.md>) or [function/execution](<../../../docs/en_GB/04.Native C++ Development/03.Functions Events and Execution.md>) section.
- For native runtime architecture, read [Build and Module Layout](<../../../docs/en_GB/04.Native C++ Development/01.Build and Module Layout.md>); for Standard/native Lua semantics, also use [ludork-lua](../ludork-lua/SKILL.md).

Use those pages as the protocol reference; do not maintain a second macro/decorator catalogue in agent instructions.

## Change the source of truth

Lua implementation belongs in `.lua`, public LuaLS declarations in mirrored `Scripts/stub/**/*.d.lua`, and handwritten editor metadata in sibling `_meta.lua`. Metadata must return pure-data `_METADATA` and execute only in a separate editor Lua state. It must not run gameplay, use runtime objects, or depend on external side effects.

Core metadata and stubs are generated from annotated C++ declarations. Edit those declarations or `ScriptTools/core_bindgen/`, then regenerate through the normal build. Never hand-edit `Sample/Scripts/Engine_meta.lua`, `GlobalCore_meta.lua`, `GlobalFunctions_meta.lua` or generated native stubs.

Generic conversions belong in recursive adapters, traits or declarative annotations under `LudorkRuntimeBinding`; bindgen must not special-case business module/type/member names. Include full declarations for cross-module dependencies. Keep C++ registration, LuaLS stubs and metadata consistent, including free-function grouping and `metadata = false` exclusions.

## Review the protocol at its boundaries

- Keep entries directly under their type, with ordered `attrs`, full module/type references in ordered direct `bases`, and exact schema types. Add fields to their owning type, not an aggregate wrapper.
- Defaults are finite, JSON-convertible pure data. A missing attribute default is schema-only unless a saved value exists. Container schemas determine array/object shape even for empty values. Write known shapes as `T[]`, `Dict[string, T]` or `Tuple[...]`; `Optional[T]` uses metadata type `T`. Existing `table`, `List`/`Array` and `Dictionary`/`Map` declarations remain accepted.
- Select composite controls from their actual `Pair` or `sf.*` types, preserving float, signed and unsigned numeric semantics. Do not revive `Vector*Vars`, `Pair*Vars`, `ColourVars` or `ColorVars` control hints. `RectRangeVars` adds texture-selection semantics to `sf.IntRect`.
- Keep ordered `parameters`, `["return"]` and aligned parameter defaults. `Pure` removes execution pins; return data does not imply purity. Use `Latent = true` with `LatentStates`, and `Loop = true` with `LoopNode`. Events remain event entries, not ordinary function nodes. The runtime must implement the declared execution protocol.
- Ordinary labels derive from identifiers. Only operator nodes may have a literal `Meta.DisplayName`; do not add ordinary display-name/description overrides or evaluate labels with `LOC(...)`. Runtime localisation and drop-down values follow their own contracts. `TypeAdapter` is unsupported.
- Use inline `BIND_*` options and lowercase `nil`/`true`/`false`. Ordered token lists and structured data use braces. Do not quote tokens to make empty variadic macros look like C++, or stringify property defaults as JSON. Historical quoted JSON in `meta(...)` remains accepted, but new/updated declarations use brace data. Structured metadata cannot retain nil fields; protocol nil placeholders have separate semantics.

Verify the affected declaration through generation, compilation and its Lua/editor consumer using [ludork-verify](../ludork-verify/SKILL.md). Update every affected locale page when the contract changes.
