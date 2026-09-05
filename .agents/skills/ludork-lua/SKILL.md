---
name: ludork-lua
description: Edit or review Ludork Lua runtime, Standard globals, LuaLS declarations, Script Mixins and Sample gameplay. Use for Lua semantics and gameplay contracts, including their native implementations.
---

# Ludork Lua

## Load the affected contract

Paths in commands and inline code are repository-relative. For Lua/runtime changes, read [Ludork Lua Advanced](<../../../docs/en_GB/01.Getting Started/04.Ludork Lua Advanced.md>) and [Lua Runtime and Modules](<../../../docs/en_GB/03.Lua and Blueprint Scripting/01.Lua Runtime and Modules.md>). These define native classes, containers, truth values, module boundaries and file ownership. Standard lives under `Sample/Engine/Standard`; it is not an ordinary Lua library.

Read additional pages only for the affected behaviour:

- Script Mixins: [Runtime Contract](<../../../docs/en_GB/03.Lua and Blueprint Scripting/03.Script Mixins/02.Runtime Contract.md>) and [Metadata](<../../../docs/en_GB/03.Lua and Blueprint Scripting/03.Script Mixins/04.Metadata.md>).
- Sample actors, combat, equipment or movement: [Actors, Enemies, Items and Equipment](<../../../docs/en_GB/03.Lua and Blueprint Scripting/06.Sample Gameplay/03.Actors Enemies Items and Equipment.md>) and the affected type's API page.
- Attributes, abilities or Effects: [Gameplay API](<../../../docs/en_GB/03.Lua and Blueprint Scripting/07.API Reference/03.Global APIs/02.Gameplay.md>). The ability system is native `GlobalCore`; do not recreate `Global.Gameplay` modules.
- General Data or saves: [General Data and Text Config](<../../../docs/en_GB/02.Editor User Guide/05.General Data and Text Config.md>) and [Runtime Data, Configuration and Saves](<../../../docs/en_GB/03.Lua and Blueprint Scripting/06.Sample Gameplay/06.Runtime Data Configuration and Saves.md>).
- Graph execution or `_meta.lua`: [ludork-bindings](../ludork-bindings/SKILL.md). UI controllers/assets: [ludork-ui](../ludork-ui/SKILL.md).

## Implement against Native APIs

Before adding a generic Lua helper, search the relevant declarations under `Sample/Scripts/stub`: `Standard.d.lua`, `Engine.d.lua`, `GlobalCore.d.lua`, `GlobalFunctions.d.lua` and `LuaSF.d.lua`. Read matching signatures and implementations as needed; unrelated APIs need not be loaded. Reuse an API only when inputs, results, mutation, ordering, nil, equality and error semantics match. Domain-specific local functions remain appropriate. Add a missing reusable primitive once in the owning Native layer, with declarations, locale docs and focused verification.

Use `copy`/`deepcopy` for basic copies; extend their generic native protocol when a supported value cannot be copied independently. Preserve documented identity/ownership policies for resource handles. Use `Class.isInstance` and `Class.isSubclass` directly, `bool` for ordinary truth tests, and explicit nil comparisons for absence/presence or protocol sentinels. Do not alias these checks, introduce `isinstance`, compare Lua `type` results or use `next` as an emptiness test. Use `math.type` only after a number check. Native `sf.*` values do not accept tuple/list/Pair adapters; `TypeAdapter` is unsupported.

## Keep implementation and declarations separate

- Define a plain class table and finalise it with `class(definition, ...)`. Editable/inheritable defaults, component fields and spawn defaults belong on that table. `init` owns constructor arguments and per-instance state; genuine static constants/caches remain shared. Direct Script Mixins initialise private state in lifecycle callbacks, not `init`.
- Instance methods use colon syntax and camelCase. Public module/static functions use PascalCase, public class constants use SCREAMING_SNAKE_CASE, and local helpers use camelCase. Keep private helpers local rather than exporting `_name`. Update callers and mirrored stubs with every public rename.
- Complete direct `require` calls in established order, then add a blank line before member aliases. Load root native modules in `Engine`, `GlobalCore`, `GlobalFunctions` order; use full paths for other modules. Do not add directory `init.lua` aggregators, chain `require(...).Member`, or move cycle-breaking delayed loads out of their functions. Use injected `sf` directly.
- Do not alias `self` or a field/index chain rooted at `self`. Computed results, including method returns, may use locals.
- Keep public declarations and full existing API prose in mirrored `Scripts/stub/**/*.d.lua`; runtime files retain implementation annotations only. Requireable stubs declare their exact `---@meta Module.Path`; global-only stubs use bare `---@meta`. Stubs contain no `require` or runtime side effects. Use `@param`/`@return` forms, not backslash-prefixed Doxygen commands; preserve existing paragraphs and exact `sf.*` parameter types.
- Group implementation casts after a contiguous assignment block, before the first use requiring narrowing. Keep a cast earlier when a later initializer depends on it or after a real nil/instance check. Field types belong on the stub class, not each `self.field` assignment. Follow EmmyLua layout; Lua `if` bodies start on a new line after `then`.

## Preserve runtime boundaries

- Use injected `PLATFORM` for platform logic, never path separators or environment heuristics. Do not add pickle readers/writers, pickle-only APIs or JSON substitutes under pickle names; saves use the documented Ludork format.
- Log through `Global.Utils.Logging`, with default `INFO` and `LEVEL:message` format. Use DEBUG for detail, INFO for stages/timings, WARNING for recoverable problems and ERROR for unrecoverable failures. Logging does not raise. Avoid routine tick/render/input/Actor-lifecycle/node logs and direct `print`/`io.stderr` calls.
- Required resource, data and type failures propagate to the host. Do not use `pcall`/`xpcall` just to log, swallow errors or continue with defaults. A protected probe is appropriate only when an exception is an intended protocol branch and no explicit state/type/result API covers it; stable APIs need no function-existence probes.
- Gameplay changes must preserve finite Base/Current values, integer attributes, side-effect-free previews and validation of required Effects before committing combat or settlement. General Data graphs activate explicitly as synchronous abilities against `GameplayEventData.target`; save restoration rebuilds persistent abilities/Effects after batched Base restoration without replaying member graphs. Follow the linked docs for the complete contracts.

Finish with the affected checks in [ludork-verify](../ludork-verify/SKILL.md), including EmmyLua formatting and full-workspace diagnostics. Keep authoritative types exact; use a cast after a real runtime check or a narrow suppression only for a confirmed analyser limitation. Templates retain stubs; packaging removes the entire stub tree and compilation excludes `.d.lua`.
