This repository is Ludork: a 2D RPG Game Engine with Avalonia editor and Lua runtime.

The project is in active development. When a new data shape, path, or API is settled, switch to it directly and remove the old protocol. Unless the current task or this file explicitly requires otherwise, do not add compatibility reads, migrations, fallbacks, aliases, dual-read, or dual-write for old fields, paths, or interfaces.

Editor-side C#: no comments or docstrings; no unnecessary try/catch; do not declare with `var` unless the full type name is excessively long.

After changing first-party C++ sources (`.h`, `.hpp`, `.cc`, `.cpp`, and similar), format those files in place with `clang-format -i` so the repository `.clang-format` is applied. Do not format git-ignored third-party or generated C++.

### Documentation

`docs/en_GB/` is the authoritative description of project architecture and runtime behaviour for agents to consult. This file is a hard constraint list for agents; it does not replace the docs.

Before changing Lua runtime behaviour, Standard or other native globals, Core bindings, blueprints or mixins, declarative UI, or Sample gameplay, read the matching English docs first. Primary Lua entry points:

- `docs/en_GB/01.Getting Started/04.Ludork Lua Advanced.md` — Standard natives (`class`, `list` / `tuple` / `dict`, `bool`, copy, platform/filesystem and `asyncio`)
- `docs/en_GB/03.Lua and Blueprint Scripting/01.Lua Runtime and Modules.md` — module load order, native-module boundaries and Lua/stub/metadata ownership
- Plus the relevant subsection under Mixin, Blueprint, declarative UI, Sample, or API reference as needed

Standard is a native layer (implementation under `Sample/Standard/`, LuaLS surface in `Scripts/stub/Standard.d.lua`). Its class, container, `toTable` and truthiness rules live in Ludork Lua Advanced; follow that page rather than assuming ordinary Lua tables alone.

When a change alters behaviour, APIs, paths, or conventions described in docs, update every locale tree under `docs/` in the same task (at least `docs/en_GB/` and `docs/zh_CN/`) so all languages stay in sync. For Sample code under `Sample/` (especially `Sample/Scripts/`), also check `docs/en_GB/03.Lua and Blueprint Scripting/06.Sample Gameplay/` and the matching pages in other locales, plus any API pages that describe the affected behaviour. Do not leave any locale describing the old protocol.

Re-examine the necessity of every design element through the lens of the "Keep Things Simple" software engineering philosophy.

Delete test scaffolding once testing is finished.

Do not patch or edit git-ignored third-party or temporary files. That includes vendored trees such as `Sample/LuaSF/`, `Sample/lua-cjson/`, `Sample/zlib/`, `Sample/ffmpeg/`, and `Sample/ThirdPartySource/`, plus generated outputs and build or session artefacts (`bin/`, `obj/`, `Temp/`, `build/`, logs, and similar). Change first-party tracked sources, CMake or build wiring, or wrappers instead. Do not add `.patch` / `.diff` files or in-tree overlays for ignored third-party code. Unless the current task explicitly requires otherwise, do not force-add ignored paths.

## Form input styles (Map Edit is the standard)

All form `TextBox` / `NumericUpDown` controls must go through `Views/Utils/EditorInputs.cs`, matching Map Edit (`MapEditWindow`) styling:

### Editable

- API: `EditorInputs.CreateEditableTextBox()` / `EditorInputs.ApplyEditable(textBox)`
- Numeric: `EditorInputs.CreateNumericUpDown(...)` / `EditorInputs.ApplyEditable(numeric)`
- Style points:
  - `MinHeight = 34`, `Padding = 16,0`, `CornerRadius = 4`, focusable and tabbable
  - `Background = #333333`, `BorderBrush = #464646`, `BorderThickness = 1`
- CSS class: `ludork-editable`

### Read-only path / display (ReadOnly)

For browse-selected path boxes and similar controls that look like inputs but must not accept typed text:

- API: `EditorInputs.CreateReadOnlyTextBox()` / `EditorInputs.ApplyReadOnly(textBox)`
- Style points:
  - `IsReadOnly = true`, `Focusable = false`, not in the tab order
  - `CaretBrush = Transparent`, mouse cursor is an arrow (avoid the editable caret look under the Material theme)
  - `Background = #262626`, `BorderBrush = #464646`, `BorderThickness = 1`, `CornerRadius = 4`
  - `Padding = 16,0`, `MinHeight = 34`
- CSS class: `ludork-readonly`

### Exceptions

- Pure display multi-line text such as Markdown code blocks is not a form input and need not use the factories above
- Colour-picker numeric boxes may call `CreateNumericUpDown(..., stretch: false)` and then set a dedicated `Width`

### When adding controls

Do not write bare `new TextBox { IsReadOnly = true }` or ad-hoc grey backgrounds. Always call `EditorInputs` so editable and read-only appearance stay consistent.

## UI asset layout

- The declarative UI asset root is fixed at `Data/UI/Assets`; complete large UI assets sit at the top level of that directory.
- Internal widgets of a large UI live under `Data/UI/Assets/Parts/<LargeUiName>/`; widgets shared by multiple UIs live under `Data/UI/Assets/Parts/Shared/`. Do not recreate directories organised by control shape such as `Rows`, `Windows`, or `Scenes`.
- Matching Lua UI classes use the same owner layering: large UIs under `Scripts/Source/UI/`, internal parts under `Scripts/Source/UI/Parts/<LargeUiName>/`. `Ui.lua`, `UiController.lua`, `UiControlFactory.lua`, and helpers that are genuinely shared across UIs are exempt.
- Declarative UI assets use a path relative to `Data/UI/Assets`, with `/` separators and no extension, as the sole asset identity; asset JSON must not store `assetId`, nested references use `Project:<relative path>`, and the project must not generate, save, or read a UI Catalog.
- When the editor moves a UI asset it must preserve asset-local unique node names, `palette.exposed`, and `palette.category`, and automatically update nested asset references in managed JSON; the caller must update `Ui.define` keys, `require` paths, and LuaDoc paths in sync, and must not add aliases for old paths.

## Lua blueprint metadata

Lua blueprint scripts declare editor-side metadata in a sibling `_meta.lua` file. For example:

```text
Scripts/Actor.lua
Scripts/Actor_meta.lua
```

`_meta.lua` is read only by the editor and does not take part in game runtime logic. Files must obey these rules:

- The file must ultimately return a table named `_METADATA`.
- `_METADATA` may only be a pure-data table; it must not contain functions, userdata, coroutines, or other runtime objects.
- `_meta.lua` must not run game logic, touch runtime objects, or produce side effects that depend on external state.
- Exposed fields, method signatures, and editor control information are described only through string fields and nested data.
- The editor must load and read `_METADATA` in a separate Lua state; metadata files must not be executed as game scripts.

### Current metadata structure

`_METADATA` is grouped at the top level by Lua type name; under each type, entries are keyed directly by field or method name. Fields must declare `type`; decorator data uses the decorator name as the key and is not reshaped into intermediate structures such as `class`, `fields`, `nodes`, or `events`.

```lua
local _METADATA = {
    SoundFilter = {
        attrs = {
            "volume",
        },
        volume = {
            type = "float",
            default = 1.0,
        },
    },
}

return _METADATA
```

- Lua types use the project’s agreed string names, for example `bool`, `int`, `float`, `string`, `function`, `event`, `Pair`. SFML types must use the `sf.` prefix, for example `sf.Vector2i`, `sf.Vector3f`, `sf.Color`, `sf.FloatRect`.
- Each type table must include an ordered `attrs` array listing field names in the declaration order of the type’s attribute members; methods, decorators, and `attrs` itself must not appear in that list. The editor must enumerate attributes through this array and must not rely on table key order.
- A type table may declare an ordered `bases` array listing only direct base classes that participate in editor field inheritance, keeping the runtime base-class declaration order. Each base must be a full binary type reference `{ "ModuleName", "TypeName" }`; bases in the same file must not be abbreviated or written as relative module names. Runtime bases with no editable metadata must not be listed.
- Attribute fields may declare `default`; values must be Lua pure data convertible to JSON (strings, finite numbers, booleans, and nested arrays/dictionaries), and must not include functions, userdata, threads, cyclic references, or expressions that depend on Lua evaluation. A field without `default` is schema only; unless the blueprint `attrs` already store that field, the editor must not show it as an ordinary field with a default.
- Fields under `_componentTypes` are marked with `component = true`; the field’s `type` points at component metadata. Component fields still follow the default-value rules; member defaults of the component type itself are written on that component type’s attribute `default`.
- Container types use recursively composable `T[]`, `Dict[string, T]`, and `Tuple[T1, T2, ...]`: `T[]` is a homogeneous variable-length list, `Dict` allows string keys only, and `Tuple` is a fixed-length heterogeneous array. Examples include `Dict[string, Tuple[string, any]]`, `Dict[string, int[]]`, and `int[][]`. `Optional[T]` is not declared as Optional in metadata; write `T` directly.
- The editor must parse defaults and saved values according to the declared container schema first, and must not override the type from the current shape of a Lua table or JSON value—especially must not treat an empty table as list or dict on its own. `Dict` serialises as a JSON object; `T[]` and `Tuple` serialise as JSON arrays. Legacy `table`, `List` / `Array`, and `Dictionary` / `Map` declarations remain accepted, but new metadata with a known structure must use the explicit recursive types.
- Composite input controls are driven directly by the field or parameter `type`: `Pair`, `sf.Vector2f`, and `sf.Vector3f` use floating-point component inputs; `sf.Vector2i` and `sf.Vector3i` use signed integer inputs; `sf.Vector2u` and `sf.Vector3u` use non-negative integer inputs; `sf.Color` uses a colour picker; `sf.IntRect` uses a rectangle input. `Pair` is a two-component numeric control type, not the fixed heterogeneous container `Tuple`. Do not declare `Vector*Vars`, `Pair*Vars`, `ColourVars`, or `ColorVars` as control-type hints; integer pairs must be declared as `sf.Vector2i`, and unsigned integer pairs as `sf.Vector2u`. `RectRangeVars` remains for adding texture association and image-selection semantics to `sf.IntRect`.
- C++ Core exposed types use `{ "ModuleName", "TypeName" }`, for example `Optional[Actor]` as `{ "Engine", "Actor" }`; array types append `[]` to the second item, for example `{ "Engine", "Actor[]" }`. Core C++ auto-generated metadata is aggregated into `Scripts/Engine_meta.lua`, `Scripts/GlobalCore_meta.lua`, and `Scripts/GlobalFunctions_meta.lua`.
- For C++ Core `BIND_PROPERTY()`, bindgen generates `default` from safe boolean, numeric, string, or `{}` member initialisers; explicit attribute defaults use annotation pure data directly, with arrays as `default = {1, 2}`, objects as `default = {key = value}`, and recursive nesting allowed. Quoted values always mean strings; a full JSON array or object must not be wrapped as a string, and legacy stringified JSON attribute defaults error immediately. Editor semantics `Meta` that cannot be inferred from types must be written on `BIND_PROPERTY(meta(...))` and `BIND_METHOD(meta(...))` respectively; do not use a standalone `BIND_META`. Ordinary members must not declare `DisplayName` or `DisplayDesc`. Getter/setter-based properties use `BIND_METHOD(property = "PropertyName", setter = "SetterName")` on a no-argument getter; the getter return type is the property type by default, and may be overridden with `type` when it differs from the setter input. That form exposes only the property, not extra getter/setter methods; `default` always means the property default and must not be combined with execution protocol. Do not hand-edit auto-generated Core metadata.
- Structured pure data inside `meta(...)` uses a brace DSL: arrays as `meta(MoveRouteVars = {"route"})`, objects as `meta(Rely = {source = "autoSound", op = "!=", value = ""})`, with the same nesting rules; booleans use lowercase `true` / `false`. Structured metadata must not contain `nil`, because Lua tables cannot retain nil fields or array slots; `nil` is reserved for protocol tokens such as `defaults`, `outpins(...)`, and `latent(...)`. Bindgen still accepts a full JSON string for historical `meta(...)` declarations (for example `"[\"route\"]"`); new or updated `meta(...)` must not wrap an entire array or object as one string; attribute `default` does not offer that historical acceptance.
- All logically public `BIND_METHOD` entries enter Core metadata by default; only internal methods and adapter overloads that must not become a second blueprint node use `metadata = false`. That option excludes metadata only and must not change C++ registration or LuaLS stubs. Unexcluded metadata method names must not be duplicated on the same type.
- Unbound C++ declarations carry no binding marker; `BIND_IGNORE` is not supported. Dynamic-value and opaque-identity traits are declared only through `BIND_CLASS(dynamic_value = true)` and `BIND_CLASS(opaque_identity = true)`. Class decorators use inline `invalid_vars(...)` and `rect_range_vars(...)`, and callable loop protocols use inline `loop_node(...)`; the former standalone `BIND_DYNAMIC_VALUE_TYPE`, `BIND_OPAQUE_IDENTITY_TYPE`, `BIND_INVALID_VARS`, `BIND_RECT_RANGE_VARS`, and `BIND_LOOP_NODE` forms are not supported. `BIND_REGISTER_EVENT()` remains a separate event marker.
- Non-`Pure` `BIND_METHOD` uses ordinary execution flow by default and need not declare a default exec pin explicitly; named execution branches are written as inline `outpins(...)`, for example `BIND_METHOD(outpins(default = nil))` or `BIND_METHOD(outpins(success = true, fail = false))`. Latent states use inline `latent(...)`, for example `BIND_METHOD(latent(TimeUp = true), ...)`. `BIND_FUNCTION` uses the same inline execution syntax and must not split it into separate execution annotations. Ordered lists such as `defaults` on `BIND_METHOD`, `BIND_FUNCTION`, and `BIND_INIT`, and `parameter_types` on functions/methods, use brace token lists, for example `defaults = {nil, true}`, `parameter_types = {float, function}`, and must not wrap the full list as one string. Singular attribute `default` likewise uses scalar or brace pure data, without changing the protocol meaning of `nil` placeholders in plural `defaults`. Annotation values must use lowercase `nil`, `true`, `false`, not Python-style `None`, `True`, `False`. `Pure = true` must not combine with any execution protocol; `metadata = false` must not combine with node execution annotations.
- `BIND_METHOD(...)` and related binding markers are compile-time empty variadic macros; bindgen reads the tokens inside the parentheses, so `outpins(...)`, `nil`, `function`, and brace lists need not be valid C++ expressions or types, and must not be quoted merely to look like C++.
- Core bindgen must not generate bespoke binding bodies keyed by business module, type, or member name. Extra conversion capability must be implemented as generic recursive adapters, type traits, or declarative `BIND_*` annotations in `LudorkRuntimeBinding`; cross-module dependencies are satisfied by including the full type declarations in the binding headers.
- Headers that contain only free functions may use file-level `BIND_FUNCTION_GROUP(name = "GroupName")`; `BIND_FUNCTION` in that header may register only into the corresponding group table and must not also leak onto the module root. C++ registration, LuaLS stubs, and metadata must share the same grouping semantics; ungrouped `BIND_FUNCTION` continues to register on the module root.
- Functions and events describe signatures with `parameters` and `["return"]`; ordinary blueprint nodes use `type = "function"`, and event entries use `type = "event"`. `return` is a Lua reserved word and must be written as a bracket key. The array part of `parameters` lists parameter names in runtime order, with string keys holding name→type; `["return"]` uses the same structure, with the array part listing return names in blueprint output order. No return outputs must be an empty table; an explicitly listed `nil`-typed return is still an output pin.
- Functions may declare a `default` array aligned with the `parameters` array order; write `nil` when there is no default, otherwise the concrete value. The runtime writes only non-`nil` entries into `_paramDefaults`. `default` indices must align with the `parameters` name order.
- There is no `ReturnType` key; ordered name→type return data is written directly under `["return"]`. `Pure = true` means only that the node has no execution inputs or outputs, and must not be inferred from `["return"]`; ordinary execution nodes may also declare data outputs via `["return"]`. On the C++ side, pure nodes are declared with `BIND_METHOD(Pure = true)` and must not use a standalone `BIND_PURE`; non-`void` returns generate a single output named `return` from the function signature, and `void` generates an empty table.
- Latent and loop nodes must declare `Latent = true` and `Loop = true` respectively. When they carry state or loop-type parameters, those live under `LatentStates` and `LoopNode`.
- `Meta`, `ExecSplit`, `Latent`, `LoopNode`, `InvalidVars`, `RectRangeVars`, and similar keep their decorator names and field names.
- Ordinary types, fields, functions, events, parameters, returns, and blueprint variables must not introduce `Meta.DisplayName`, `Meta.DisplayDesc`, `Meta.VariableDisplayNames`, `Meta.VariableDisplayDescs`, `Meta.ParameterDisplayNames`, or `Meta.ParameterDisplayDescs`. The editor displays identifiers in UE style by splitting them and capitalising each word, for example `textureRect` as `Texture Rect`; runtime member names, metadata keys, and blueprint serialisation names stay unchanged.
- Only nodes that must display as operators, such as `ADD` and `IADD`, may use `Meta.DisplayName` for literal `+`, `+=`, and similar; do not use `LOC(...)`, `Eval`, or other evaluation, and do not invent a default `DisplayDesc` for those nodes.
- The editor must not resolve or execute `LOC(...)` strings in metadata for member, node, or pin names. Game runtime and other localisation uses such as drop-down options are outside this rule.
- New fields on a runtime type must be added to that type’s table; do not create a separate aggregated metadata set.

### Decorator conventions

Except for `TypeAdapter`, `ReturnType`, and `RegisterEvent`, decorators are represented in `_METADATA` as pure-data tables keyed by the same decorator names. `TypeAdapter` involves runtime parameter conversion and is not supported; do not simulate it in Lua metadata.

- `Meta`: kept as a `Meta` table. Includes `DropBox`, `Rely`, `PathVars`, `ProgressVars`, `SliderVars`, `RangeVars`, `MoveRouteVars`, `Transfer`, `BlueprintClassVars`, `CommonFunctionVars`, `GeneralDataVars`, `ConfigVars`, and other editor descriptions that cannot be inferred from types alone. Ordinary members’ `DisplayName`, `DisplayDesc`, `VariableDisplayNames`, `VariableDisplayDescs`, `ParameterDisplayNames`, and `ParameterDisplayDescs` are not used; the only exception is a direct literal `DisplayName` on operator nodes.
- `ReturnType`: no same-named key. Ordered return name→type data is written under `["return"]`; whether a node is pure is decided only by `Pure`.
- `ExecSplit`: kept as an `ExecSplit` table; the array part lists branch names in execution-pin order, and string keys hold branch name→match value.
- `Latent`: kept as a `Latent` table; the array part lists state names such as `Started` and `Finished` in execution-pin order, and string keys hold state name→output value type; the runtime must consume these states and resume suspended blueprint execution when conditions are met.
- `LoopNode`: kept as a `LoopNode` field or table, declaring the loop type as a string such as `ForLoop` or `ForEach`; the runtime must iterate the loop body according to that type and wire execution pins such as `LoopBody` and `Completed`.
- `InvalidVars`: kept as an `InvalidVars` table. Hidden fields must not appear in the blueprint property editor and must not be serialised as ordinary editable fields.
- `RectRangeVars`: kept as a `RectRangeVars` table, declaring the range or resource parameters for the field.
- `RegisterEvent`: no same-named key. `BIND_REGISTER_EVENT()` and equivalent event registration produce `type = "event"`, keeping `parameters`, `["return"]`, and `ExecSplit`. When there is no explicit execution output, default to `ExecSplit = { "default", default = "nil" }`. Events may only be blueprint event entries and must not be exposed as ordinary function nodes.

Purely declarative decorators (`Meta`, `InvalidVars`, `RectRangeVars`) and `["return"]` are consumed mainly by the editor; execution-bearing decorators (`ExecSplit`, `Latent`, `LoopNode`) and `type = "event"` declarations describe execution protocol in `_METADATA` and must be consumed by the blueprint runtime. `_METADATA` still must not contain functions or runtime objects; runtime behaviour must come from the actual Lua/C++ node implementations.

## Lua script modules

- Before adding any Lua `local function`, inspect the existing Native API surfaces in `Standard.d.lua`, `Engine.d.lua`, `GlobalCore.d.lua`, `GlobalFunctions.d.lua`, and `LuaSF.d.lua`, plus the relevant Native implementation when necessary. Reuse a Native API directly only after confirming that its input, return value, mutation, ordering, nil, equality, and error semantics completely cover the helper; do not add a wrapper or reimplementation when they do. If a missing capability is generic and reusable, implement it once in the appropriate Native layer and update its stubs, all locale docs, and tests. Keep a local helper only for domain logic or genuinely different semantics, and never replace code from name similarity alone.
- The Lua tree does not mimic a Python package: do not create `__init__.py` counterparts, and do not add `init.lua` aggregation entries for directories.
- Engine, GlobalCore, and GlobalFunctions load only through their root modules, in the fixed order `require("Engine")`, `require("GlobalCore")`, `require("GlobalFunctions")`; free functions under GlobalFunctions are accessed from the `Components`, `UI`, `NodeGraph`, and `Manager` groups. Other Global and Source modules load explicitly by full path.
- Runtime Lua lives in ordinary `.lua` files and keeps only logic and implementation-state annotations such as `---@cast`, local variable types, and local/private function signatures.
- Within a contiguous local or assignment block, write the Lua statements first and group `---@cast` (and any remaining implementation annotations) immediately after that block, before the first use that needs the narrowed types. Do not interleave one statement with one annotation. Keep a cast in place when a later initializer in the same block depends on it, or when it narrows after a nil or instance check. Field types belong on the matching stub `---@class` / `---@field` list, not as `---@type` on each `self.field =` in `init`.
- Do not declare a local as an alias of `self` or of a direct field/index chain rooted at `self`; forms such as `local gameMap = self` and `local camera = self._camera` are forbidden. Use `self` or the member directly. Locals that hold a method return value or another genuinely computed result are not aliases under this rule.
- Declare instance methods with colon syntax, for example `function Actor:update(deltaTime)`. Do not write dot-form declarations with an explicit receiver such as `function Actor.update(self, deltaTime)`.
- Public documentation and LuaLS API declarations live only in same-path `.d.lua` files under `Scripts/stub`; declaration files that correspond to a `require` must use an explicit EmmyLua module declaration matching the relative path (for example `Global/Pool.d.lua` writes `---@meta Global.Pool`); `Standard.d.lua`, `Class.d.lua`, and `LuaSF.d.lua`, which declare global API only, use bare `---@meta`. Declaration files must not contain gameplay logic, initialisation side effects, or `require`. Doxygen commands are written as `@brief`, `@param`, `@return`, and other `@` forms; do not keep a leading backslash before the command.
- Editor metadata lives only in a sibling `_meta.lua` beside the runtime script; `.lua`, `_meta.lua`, and `Scripts/stub/**/*.d.lua` must not be mixed.
- Project templates must keep the full `Scripts/stub/**/*.d.lua` tree for development tools; game packaging must delete the entire `Scripts/stub` directory, the Lua compiler must ignore `.d.lua`, and the final package must not retain any `.d.lua`.
- After editing Lua, run the EmmyLua formatter on every changed `.lua` and `.d.lua` file before verification. Use EmmyLua's built-in formatter through the editor or its `textDocument/formatting` language-server request; do not substitute an unrelated Lua formatter.
- After changing Lua, run full-workspace EmmyLua diagnostics and keep errors, warnings, and hints at zero whenever reasonably possible. Fix the underlying type or control flow instead of weakening contracts or adding broad ignores; use a narrowly scoped suppression only for a confirmed analyser limitation.
- Never silence EmmyLua diagnostics by widening a type that is already authoritative in `_meta.lua`, generated Core metadata, a native binding signature, or another settled schema/API into a union with unrelated types. Keep the authoritative type exact and fix the control flow, call site, or override model; use a cast only after a real runtime check, or a narrowly scoped suppression for a confirmed analyser limitation. A union is valid only when the runtime protocol genuinely accepts every member and the corresponding source of truth describes that contract; satisfying the analyser alone is not evidence for a union.

## Lua platform detection

- Platform is injected by a CMake compile macro as the global string `PLATFORM`; Lua scripts must not infer the operating system themselves.
- Agreed values include `win32`, `darwin`, `ios`, `android` and `ohos`; other platforms use the lower-case CMake system name.
- All platform-related runtime logic must read `PLATFORM`, and must not judge platform indirectly via path separators, environment-variable presence, or similar heuristics.

## Persistence: pickle is not supported

- Do not implement pickle serialisation, deserialisation, or file compatibility in Lua; there must be no pickle reader or writer.
- Module members, APIs, data paths, and tests that exist only to serve pickle (for example `Engine.saveData` / `Engine.loadData`) are out of scope.
- Do not silently emulate a pickle API with JSON or another format; if persistence is needed later, define a Ludork data format and interface for this project.

## Lua class fields and init

Class Default Object (CDO) attribute defaults belong on the **class body**; `init` is responsible for non-CDO per-instance runtime state. In this repository `Class.create` inherits class-table fields through `__index = class`, so defaults that are editable, inheritable, or overridable by subclasses or Blueprints must be written on the class table returned by `Class.create`:

```lua
local Character = Class.create("Character", Actor)
Character.direction = 0
Character.directionFix = false
```

- **Do not** put CDO attribute defaults only as `self.field = default` inside `init`. Otherwise the class table has no visible default, subclasses cannot override with `Subclass.field = ...`, and later instantiation and editor field discovery break.
- Constants, caches, shared state, or public static APIs that truly have class-level semantics may remain on the class table or in module scope; do not mechanically move them into `init` merely because they are not CDO defaults.
- Non-CDO instance state must be initialised per instance. Ordinary classes write `self.field = ...` in `init`; Direct Script Mixin runtime forbids declaring `init`, so private instance state belongs in the corresponding instance lifecycle entry points.
- `init` may: apply constructor arguments, initialise non-CDO private runtime state (often `_`-prefixed, such as `_isMoving`, `_map`), and perform established normalise/clone work (for example normalising `autoSoundParams` in `Actor:init`).
- Meta-editable fields, `_componentTypes`, spawn `default*` fields, and dataclass component fields (such as `lightColour`, `className`) all belong on the class table.
- Correct example: `Character.lua` (`direction` and similar on the class table; `init` only sets `_rectSize` / `_sx` / `_sy`). Incorrect example: assigning `material` or public component fields only inside `init`.

## Gameplay ability system

- General Data runtime shape is `params`, `members`, optional `events`, and optional member `_graph`. `linkedType` is unsupported and must fail validation; do not add runtime object wrappers, implicit member-graph dispatch, or compatibility reads.
- Saving General Data generates `Source.Configs.GeneralDataTypes` and its stub. Runtime gameplay objects create the generated `<Type>AttributeSet` through `GeneralDataTypes.Create(typeName, memberID, memberData)` so existing General Data field names remain the authoritative attributes.
- A Battler exposes its generated Attribute Set as `attributes` and its `Global.Gameplay.AbilitySystemComponent` through `getAbilitySystemComponent()`. Numeric gameplay changes go through numeric base setters or `GameplayEffectSpec`; do not add parallel component-backed attribute mutation paths.
- Numeric Attribute Set Base and Current values must be finite. An `int` attribute must remain a Lua integer at every write and recalculation; a `float` attribute accepts any finite Lua number.
- Attribute listeners receive Current old/new values plus an `AttributeChange` whose source is `Base`, `Effect`, or `Constraint`; permanent progression must derive from `oldBase` / `newBase`, not temporary Current changes. Numeric constraints run after modifier aggregation and must not mutate Base. Player HP Current is constrained to `0..resolved MAXHP`.
- Gameplay abilities are synchronous and always return `GameplayAbilityResult`. `GameplayEventData` carries `instigator`, `target`, `eventTag`, and `payload`; hierarchical event and owned tags use dot-separated prefixes.
- Gameplay Effects support only `Instant` and `Infinite` duration, `None` and `Aggregate` stacking, and `Add`, `Multiply`, and `Override` modifiers. A modifier may declare a finite `minimum`; Infinite recalculation applies the highest matching minimum after aggregation, while an Instant modifier clamps the prospective Base before writing it. Instant Effects must not grant tags or abilities. Extend this contract deliberately instead of emulating unimplemented UE GAS systems.
- `None` Effect Specs require exactly one stack; `Aggregate` Infinite Effects require a non-empty ID. Numeric Base setters, Instant application, first Infinite application, Aggregate stack changes, and Effect removal must preview every resulting Current before mutation. `validateGameplayEffectSpec` performs the same validation/preview without mutation and is the precommit boundary for required gameplay Effects.
- Gameplay Blueprint nodes whose attribute name is chosen at runtime expose dynamic numeric value pins as `any`; the Ability System still requires a number and enforces the selected schema's finite `int` or `float` contract.
- Mota battle calculation prepares and validates counter-damage and Game Over Specs. Enemy collision prepares and validates win rewards and State Specs before `CommitResult(result)`; do not commit combat damage first and discover required settlement errors afterwards.
- Domain, Flank, and Blockade Effects grant `Gameplay.Movement.Hazard`. Both movement commit and `MovementDangerGrid` consume `Source.MovementSpecials.Preview`; do not duplicate their spatial rules in the danger grid.
- General Data member graphs are abilities, not Actor Blueprint events. Activate them explicitly with `Source.Gameplay.GeneralDataGraphAbility`; the graph parent and `eventData` keyword argument are the same `GameplayEventData`. A null start means no graph implementation. These graphs must not use latent nodes, and execution errors propagate.
- General Data graph nodes that act on `GameplayEventData.target` must not look up a global Player. Item `onUse` targets the activating Player, while Item `onDrop` targets the spawned Item Actor.
- Enemy defeat extension uses `Actor.BlueprintEvent` to dispatch the Actor `onDefeat` event. Only its completion callback runs fixed settlement; `afterBattleVarChanges` is prevalidated during collision, then reevaluated after the event with fresh `current` and `value` locals before commit.
- Player save restoration constructs with Class initial equipment disabled, restores all numeric Bases in one batch, then reconstructs Item abilities, equipment Effects and State Effects without replaying member graphs. Do not let Class defaults or `onEquip` run as a load side effect.
- `Engine.BPBase.ExecuteGraph(graph, eventName, keywordArguments?, localGraph?, onComplete?)` is the sole explicit graph runner. It is a Lua runtime API with `metadata = false`, not a Blueprint node; do not add a second General Data copier or implicit graph runner.

## Lua logging

- Runtime Lua logs only through `Global.Utils.Logging`; production scripts must not call `print` or `io.stderr` directly.
- Default level is `INFO`, format fixed as `LEVEL:message`. Use `DEBUG` for per-file diagnostic detail, `INFO` for key stages and timings, `WARNING` for recoverable data problems that still need attention, and `ERROR` only when recording unrecoverable failures.
- `Logging.error` records a log line only and does not replace `error` or `assert`. Do not introduce `pcall` / `xpcall` merely to log, swallow errors, or degrade required data failures into defaults.
- Do not add routine logging on high-frequency paths such as tick, render, input, per-Actor lifecycle, or per-blueprint-node execution.

## Sample Lua style and documentation

After changing Sample code, read the English docs and update every locale’s matching pages in the same task—especially `docs/en_GB/03.Lua and Blueprint Scripting/06.Sample Gameplay/` and any API pages that describe the affected behaviour (see Documentation at the top of this file).

- Static functions on Lua class tables and public functions exported by modules use PascalCase and must not start with `_`, for example `WindowShop.GetDefaultRects()`, `UiLayout.GetCenteredRect()`. Instance methods use colon calls and camelCase, for example `windowShop:getVisible()`; local functions likewise use camelCase. Helpers reused only inside a module should be local functions and must not be attached to the class table or module export table as `_name`.
- When adding or changing class-table static functions or module public functions, update every call site and the matching `Scripts/stub/**/*.d.lua` declarations in the same change, and remove the old names outright; do not keep aliases or compatibility entry points for old camelCase or `_`-prefixed names. Public class constants use SCREAMING_SNAKE_CASE, for example `WindowShop.SHOP_MODE_BUY`.
- Global `copy` and `deepcopy` are the low-level copy primitives from Standard; gameplay scripts and Utils must reuse them first, and must not add type-specific basic copy helpers such as `copyRect`, `copyScale`, or `cloneVector`. If a supported LuaSF/native value type cannot obtain an independent copy, extend the generic copy protocol in Standard/LuaSF, the LuaLS stubs, and the underlying tests; do not bypass by type in upper layers. Resource handles and exclusive-ownership userdata follow only their underlying, documented copy policy.
- Within one scope, finish all direct `require` calls first in their established relative order, leave a blank line, then declare module member aliases such as `Engine.Input`; do not chain `require("Module").Member`. Delayed loads placed inside functions to break cycles must stay inside those functions.
- All Lua type checks use `Class.isInstance` directly: pass a finalised Ludork/native class table for inheritance-aware checks, or an exact raw Lua type name such as `"string"`, `"table"`, or `"function"` for primitive checks. Class inheritance checks use `Class.isSubclass`. Do not create local aliases for these functions, and do not add a global `isinstance`.
- Do not compare the result of Lua `type` with `==` or `~=`. Use `Class.type` only when code needs the resolved type value itself, and use `math.type` only to distinguish integer and float after a numeric `Class.isInstance` check. Stable interfaces do not defensively probe for function existence; wrong types should fail on the real call.
- Ordinary truthiness checks use the global `bool`, so `nil`, `false`, `0`, empty string, and empty table match the intended falsy semantics; do not use `next(value) == nil` / `next(value) ~= nil`. Keep explicit nil comparisons only when you must distinguish “not provided” from `false` / `0` / `""` / `{}`, table-key presence, protocol sentinels, or metadata nil semantics.
- In Lua `if` statements, break after `then`; do not write single-line `if ... then ... end`.
- Lua layout and line wrapping, including table-constructor layout, follow the EmmyLua formatter output. Do not manually reshape formatted code merely to preserve an older layout preference.
- `sf` is registered by the runtime as a global object; do not `require` it or create a local alias with `assert(_G.sf, ...)`; use the global `sf` directly.
- Do not use `pcall` / `xpcall` to swallow errors, degrade project resource or data failures into defaults, or simulate `finally` and continue; errors must propagate to the host. Protected calls with clear probe semantics are allowed only when the exception itself is a normal protocol branch and no explicit type, state, or return-value API exists for the check.
- When a public API already has documentation, keep the full description, paragraphs, and `@param` / `@return` information in the matching `Scripts/stub/**/*.d.lua`; do not replace it with a short summary. Descriptions that formerly assumed tuple/list adapters for SFML types must state the `sf.*` types Lua actually accepts. Do not invent documentation for new functions that have none.
- `TypeAdapter` is not supported: do not add a substitute implementation or metadata in runtime Lua or `_meta.lua`. APIs that accept SFML values take the corresponding `sf.*` objects only; do not keep tuple/list/Pair adapter entry points.
