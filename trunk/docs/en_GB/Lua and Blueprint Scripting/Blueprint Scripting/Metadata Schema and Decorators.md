# Metadata Schema and Decorators

`_METADATA` is the pure-data schema that describes Blueprint fields, function signatures, editor controls and execution protocols for Lua and C++ types. Every metadata file must return a pure-data table named `_METADATA`.

```lua
local _METADATA = {
    SoundFilter = {
        moduleReturn = true,
        attrs = {
            "volume",
        },
        volume = {
            type = "float",
            default = 1.0,
        },
        apply = {
            type = "function",
            parameters = {
                "self",
                "source",
                self = { "Source.SoundFilter", "SoundFilter" },
                source = { "Engine", "SoundSource" },
            },
            default = { [1] = "self" },
            ["return"] = {},
            Pure = false,
        },
    },
}

return _METADATA
```

## Types and fields

Every type table lists its editable fields in an ordered `attrs` array; table key order is not significant. Each field requires a `type` and displays an ordinary value only when the Blueprint already stores one or metadata provides a `default`.

- **Primitives**: `bool`, `int`, `float`, `string`, `function`, `event`, `any` and `Pair`.
- **SFML and Core types**: SFML values use the `sf.` prefix. Core and Lua types use `{ "Module", "Type" }`, with `[]` on the second item for arrays.
- **Recursive containers**: `T[]` is a variable-length list, `Dict[string, T]` is a string-keyed dictionary and `Tuple[T1, T2, ...]` is a fixed-length heterogeneous array (for example `Dict[string, Tuple[string, any]]`, `Dict[string, int[]]` or `int[][]`).
- **Component and composite controls**: `component = true` marks component fields. Composite controls are selected from their real type (vectors, colour, rectangles and `Pair`, where `Pair` is a two-component numeric control rather than a heterogeneous `Tuple`). `RectRangeVars` adds texture-range selection to `sf.IntRect`.

The capitalised `Dict[...]` and `Tuple[...]` tokens describe metadata schemas, not constructors for Standard's lower-case runtime `dict` and `tuple` containers. Metadata defaults must be JSON-convertible pure data using plain Lua tables rather than native `list`, `tuple` or `dict` values. Runtime pins that accept native containers must declare their qualified type (such as `{ "_G", "list" }`) or include it in a union; `T[]` pins accept Lua arrays and require an explicit conversion node for a native `list`.

The declared schema always governs serialisation shape, even for empty containers: `Dict` serialises as a JSON object, while lists and `Tuple` serialise as JSON arrays. Use an explicit recursive type whenever a shape is known. Runtime calls must supply the declared types; see [Runtime Value Boundaries](<../../Native C++ Development/Runtime Value Boundaries.md>) for conversion rules.

### Value literals and unset inputs

- A `string` field preserves literal text verbatim, including `"nil"`. Strings nested inside saved containers stay literal.
- Whole-value expressions evaluate once, so `nil` in an `any` expression yields Lua `nil`.
- Unset node inputs save as JSON `null` and fall back to the declared default when present. JSON-value editors accept `null` directly; clearing a function expression leaves it unset.
- An `any` input is edited as text: on focus loss, text that parses as a JSON literal (such as a number) saves as that literal, while other text saves as an expression string.

### Plain-text node inputs

**Convert to Plain Text Inputs** bypasses editor selectors and numeric controls while enforcing the same schema:

| Schema type | Accepted plain-text input |
|---|---|
| `string`, `file` | Preserves all text verbatim, including whitespace, blank input and `null`. |
| `int` | Signed 64-bit integer with no fractional part. Blank input is unset. |
| `float`, `double`, `number` | Finite number using `.` as the decimal separator. Blank input is unset. |
| `bool` | `true` or `false`. Blank input is unset. |
| Vectors, `Pair` | Valid JSON component array (e.g. `[1, 2]`). Blank input is unset. |
| `sf.Color` | JSON array of four integers in `0..255`. Blank input is unset. |
| `sf.FloatRect` | JSON array of four numbers. Blank input is unset. |
| `sf.IntRect` | Nested JSON array, such as `[[0, 0, 32, 32]]`. Blank input is unset. |
| Containers and unions | Valid JSON; unions use the declared `$type`/`$value` wrapper, including inside containers. Blank input is unset. |
| `any` | Standard JSON-or-expression conversion, preserving blank text and quoted JSON strings; enter `null` to leave unset. |
| Functions, events, objects | Standard runtime expression and object-reference semantics. |

Invalid plain text is kept as an editor-session draft and never saved as a parameter value. It blocks saving and returning to typed inputs until fixed. Plain-text mode and drafts are window-local and do not modify metadata or Blueprint JSON.

## Blueprint attribute declarations

A Blueprint JSON document stores attribute values in `attrs` and declares fields introduced by that Blueprint in the sibling `attrDefs` object. Omit `attrDefs` when the Blueprint introduces no new fields. Each entry requires `type` using the same primitive, qualified, enum or recursive schema as metadata; `file` fields may also declare `base` to constrain the asset selector while storing a complete `/Game/Assets/...` path.

```json
{
  "attrDefs": {
    "rewardCount": { "type": "int" },
    "stairChoice": { "type": { "enum": "Enums.StairDirection" } },
    "offsets": { "type": { "list": "sf.Vector2i" } },
    "icon": { "type": "file", "base": "Images" }
  },
  "attrs": {
    "rewardCount": 3,
    "stairChoice": "Up",
    "offsets": [[0, 1]],
    "icon": ""
  }
}
```

An `attrDefs` entry contains only `type` and optional file `base`; values and initial defaults belong in `attrs`. A declaration may omit a value, and both editor and runtime preserve its declared type rather than inferring one from the value.

- **Inheritance**: Resolve declarations from Lua/native metadata, the active Script Mixin and ancestor Blueprints before adding local `attrDefs`. A child inherits both type and default: it may override the value in `attrs`, but must not repeat or alter an inherited declaration in `attrDefs`. Local names must not collide with existing fields, methods or reserved runtime names (for example, `speed` or a Mixin's `needKeyCount` is a value override, not a new declaration).
- **Strict validation**: Every saved attribute and nested typed record member must be declared and match its schema. Unknown fields, missing definitions, invalid schemas and conflicting declarations fail loading with the Blueprint path and field name; neither editor nor runtime infers missing declarations or converts legacy untyped shapes.
- **Other graph assets**: Map `BPClassVarChanged` entries store values only and validate against the Actor class's inherited schema. Graph-only Common Functions and General Data member graphs have no class declarations.

## Enum schemas

An enum schema references one module under `Scripts/Enums` whose source holds a literal scalar table and its EmmyLua annotation (see [Enum modules](<../Lua Runtime and Modules.md#enum-modules>)). Reference it by its `require` path in any field, parameter, return or nested container/union schema:

```lua
stairDirection = {
    type = { enum = "Enums.StairDirection" },
    default = "None",
}
```

### Source module and scalar-type rules

- **Allowed syntax**: An enum module may return a literal table directly or assign one local literal table and return that variable. Comments, annotations, optional semicolons and one pair of parentheses around a scalar literal are allowed. Function calls (including `require`), calculations, table mutations, other returned variables and extra statements are rejected.
- **Keys and value categories**: Keys must be non-empty strings, and all values must share one category: strings, booleans or finite numbers. Without `valueType`, an all-integer table resolves to `int`, while any floating-point literal (including `1.0`) resolves to `float`.
- **Explicit `valueType`**: Optional `valueType` (`"string"`, `"bool"`, `"int"` or `"float"`, e.g. `{ enum = "Enums.GeneralData.Item", valueType = "string" }`) allows empty enum tables and fixes the scalar type in advance. Non-empty constants must match it (integer literals are accepted for explicit `float`). An empty module without `valueType`, an incompatible constant, an unknown schema property or a missing module fails validation, including inside nested containers. `valueType` constrains only the scalar type, not membership in the current option list.

### Selector, defaults and live resolution

The editor selector lists keys in declaration order and saves the underlying scalar value directly — not the key name or a Lua expression — without requiring `Meta.DropBox`. Equal values display the first matching key. Values absent from the current table stay saved with an unknown-value message and are valid if their scalar type matches; enums do not impose runtime membership checks.

- **Defaults**: An explicit `default` takes priority as the scalar value. Without one, new node inputs and container items use the first option, or the scalar type's default (`""`, `false` or `0`) when the enum is empty. An attribute without a default stays schema-only until a value is saved. String enum selectors include **— None —** (storing `""`), while unknown non-empty values stay visible and preserved.
- **Ordinary enums**: The editor rereads the source file whenever the field displays or the selector opens (including after regaining window focus) without executing gameplay modules.
- **Project-managed enums**: The editor resolves four `valueType = "string"` enums directly from the live project catalogue (including unsaved edits), before source generation:

| Managed enum | Selects |
|---|---|
| `Enums.GeneralDataKey` | General Data type keys |
| `Enums.GeneralData.<TypeName>` | Members of a General Data type |
| `Enums.Animation` | Animation keys |
| `Enums.Particle` | Particle keys |

A deleted managed module fails validation even if an old generated file exists on disk. An invalid enum reports its module path and preserves the current value.

### Enum-keyed dictionaries and plain text

Dictionary schemas can declare a string enum key independently of their value schema:

```lua
type = {
    dict = "int",
    key = { enum = "Enums.GeneralData.Item", valueType = "string" },
}
```

- Omitting `key` uses ordinary string keys. An explicit `key` must be `"string"` or an enum declaring `valueType = "string"`; numeric, boolean, container and untyped enum keys are rejected.
- Values use their own recursive schema, and JSON serialises as an object with the selected scalar strings as keys.
- Adding an enum-keyed row waits for a non-empty unique key before updating saved data; duplicate keys are rejected. `DictKeyMeta` refines unrelated key controls without replacing the declared key type.
- **Plain-text mode and connections**: **Convert to Plain Text Inputs** edits the underlying scalar value (for example, enter the number for an integer enum). The schema preserves enum identity, including inside a union's `$type` descriptor, while the stored value is the scalar. [Typed connections](<Execution Flow Events and Variables.md#typed-connections>) check compatibility using that underlying scalar type.

Native enums and marked constant maps generate equivalent modules such as `Enums.Engine.Direction`, `Enums.GlobalCore.WeatherType` and `Enums.sf.Keyboard.Scan`; see [Generated enum modules](<../../Native C++ Development/Generated Metadata and Stubs.md#generated-enum-modules>).

## Union schemas and stored literals

A union declares its allowed branches as `{ union = { T1, T2 } }` (displayed as `Union[T1, T2]`) and composes with `{ list = T }`, `{ dict = T }` and `{ tuple = { T1, T2 } }` — the structured forms of `T[]`, `Dict[string, T]` and `Tuple[...]`.

A declared default chooses the initial branch and value. Otherwise, a new node selects the first branch that has a valid literal default; branches without one stay unset. Switching branches resets the value to the new branch's default.

```lua
type = { union = { "sf.Vector2f", "sf.Vector2u", "nil" } },
default = { ["$type"] = "sf.Vector2u", ["$value"] = { 64, 32 } },
```

This saves in JSON as `{ "$type": "sf.Vector2u", "$value": [64, 32] }`. `$type` identifies the branch using its qualified name or recursive container schema (such as `{ "list": "sf.Vector2u" }`), distinguishing types with identical JSON shapes.

Explicit `nil` serialises as `{ "$type": "nil", "$value": null }` in JSON and `{ ["$type"] = "nil" }` in Lua metadata, distinct from an unset value (which falls back to defaults). Use the `$type`/`$value` wrapper only where a schema declares a union, including nested unions. Runtime calls receive the unwrapped value; invalid values report the affected field or parameter.

## Inheritance

`bases` is an ordered array of direct metadata bases written as `{ "Module", "Type" }`. List only bases that participate in editor field inheritance. Script Mixin metadata cannot declare `bases`.

When a Lua module returns its class directly, set `moduleReturn = true` in that class's metadata table and keep exactly one type in the metadata file. Class selectors and new Blueprint parents then use the module path (such as `Source.MapActors.Enemy`). A class exposed as a named member omits `moduleReturn` and uses its full reference (such as `Engine.Actor`). Metadata type references — including `bases`, fields and node pins — always use the explicit `{ "Module", "Type" }` pair (for example `{ "Source.MapActors.Enemy", "Enemy" }`).

## Functions and events

`parameters` and `["return"]` combine an ordered name array with name-to-type entries. The optional `default` array aligns by index with `parameters`. `Pure` is explicit, while `ExecSplit`, `Latent`, `LoopNode` and `type = "event"` declare the graph runtime execution protocol.

- **Instance methods**: Every ordinary instance method declares `"self"` as its first parameter with the owning class's explicit `{ "Module", "Type" }` reference and `default[1] = "self"` (for example `{ "Source.Scenes.SceneMap", "Scene" }`). Its receiver pin is visible in every Blueprint context; see [Instance method receivers](<Execution Flow Events and Variables.md#instance-method-receivers>). Include the receiver slot when indexing defaults and saved parameters. Static functions and events omit `"self"`. Lua colon-call signatures and native method declarations keep their normal argument lists.
- **Pin ordering and returns**: The array part sets pin order and string keys set pin types. A `nil` slot in `default` means no default. A `nil` type named in `["return"]` creates an output pin, whereas an empty `["return"] = {}` declares no outputs. Only `Pure = true` removes execution pins.
- **Unset defaults**: Use `defaultUnset = { "parameterName" }` when a parameter's initial value must stay `nil` instead of receiving an editor-generated literal (for example, `Scene.recordActorPosition` sets `defaultUnset = { "position" }` so it defaults to the Actor's current position). Names in `defaultUnset` are unaffected by receiver index shifts.

A static function declares only its own arguments:

```lua
calculate = {
    type = "function",
    parameters = {
        "value",
        "clamp",
        value = "float",
        clamp = "bool",
    },
    default = {
        nil,
        true,
    },
    ["return"] = {
        "result",
        result = "float",
    },
    Pure = true,
}
```

## Execution declarations

- **`ExecSplit`**: Orders execution outputs and maps them to runtime return values, e.g. `ExecSplit = { "success", "fail", success = true, fail = false }`.
- **`Latent = true`**: Marks graph suspension. Ordered `LatentStates` maps resume outputs to stage values, and the return type must be `{ "Engine", "AsyncOperation" }`. See [Execution semantics](<Execution Flow Events and Variables.md#execution-semantics>).
- **`Loop = true`**: Marks a loop node; `LoopNode = "ForEach"` or `"ForLoop"` selects the runtime iteration protocol.
- **`type = "event"`**: Creates an event entry node using the same parameter/return format. Without an explicit `ExecSplit`, the event receives `ExecSplit = { "default", default = "nil" }` (the string `"nil"` is the metadata token for the default branch).

> **Important:** Execution declarations are contracts with the graph runtime. The Lua or C++ implementation must return the declared split values, resume the declared latent states or drive the declared loop protocol.

## Editor names and decorators

Editor labels for types, fields, functions, events, pins and Blueprint variables are derived automatically from their identifiers (for example, `textureRect` displays as **Texture Rect**). Metadata keys, runtime member names and saved Blueprint names stay unchanged.

Do not declare `DisplayName`, `DisplayDesc`, `VariableDisplayNames`, `VariableDisplayDescs`, `ParameterDisplayNames` or `ParameterDisplayDescs` on ordinary members; the editor never evaluates `LOC(...)` or other expressions to derive schema labels or descriptions. The sole exception is operator nodes such as `ADD` and `IADD`, which may declare a literal `Meta.DisplayName` (such as `+` or `+=`) without `LOC(...)` and without a default `DisplayDesc`.

`Meta` supplies editor semantics that cannot be inferred from the declared type: `DropBox`, `PathVars`, `PathRoot`, `PathFilter`, `ProgressVars`, `SliderVars`, `RangeVars`, `MoveRouteVars`, `Transfer`, `BlueprintClassVars`, `CommonFunctionVars`, `ConfigVars`, `InstVar`, `InstVarValue`, `DictKeyMeta`, `ItemMeta`, `InvalidVars` and `RectRangeVars`. Preserve decorator names and ordered data directly; do not wrap them in invented `fields`, `nodes` or `events` containers.

### Resource and data path selectors

| Decorator | Behaviour |
|---|---|
| `PathVars` | Selector root. For game resources, supply a full logical root such as `/Game/Assets/Sounds`; the field stores the full logical path. |
| `PathRoot = "Project"` | Selects project files such as Script Mixins. |
| `PathRoot = "Data"` | Selects files under a project-relative `PathVars` directory (such as `Data/Subtitles`) and stores the full `Data/...` path. References must stay inside the selector directory without traversal segments. |
| `PathFilter` | File pattern filter, e.g. `PathFilter = "*.json"`. |

Function metadata targets a named parameter by table mapping, such as `PathRoot = { subtitleFileName = "Data" }`.

### Game-variable selectors (`InstVar` and `InstVarValue`)

- **`InstVar`**: Marks a string field or parameter as a game-variable name, replacing text input with a searchable selector populated from current declarations (including unsaved edits). Set `InstVar = true` for all types, `InstVar = { types = { "int", "float" } }` to restrict to listed metadata types, or `InstVar = { "valueName" }` in function metadata to target named parameters.
- **`InstVarValue`**: Links a field or parameter to the selected variable's type so it adopts that type's editor control. Specify the sibling field name as a string (e.g. `InstVarValue = "openConditionName"`) or a parameter map in function metadata (e.g. `InstVarValue = { value = "valueName" }`). This affects editor controls only; runtime reads and writes use the actual game-variable table.

```lua
openConditionName = {
    type = "string",
    Meta = {
        InstVar = {
            types = {
                "int",
                "float",
            },
        },
    },
}
openConditionVal = {
    type = "int",
    Meta = {
        InstVarValue = "openConditionName",
    },
}
```

### Container and control decorators

Container decorators refine controls at the immediate container layer without altering the schema or JSON shape:

- **`DictKeyMeta`**: Applies decorators to each string key.
- **`ItemMeta`**: Applies decorators to each immediate list item or dictionary value. When the item is a `Tuple`, `ItemMeta.TupleMeta` maps 1-based tuple positions to per-slot decorators.
- **`DropBox`**: Provides ordered literal drop-down choices when no shared enum module exists; prefer an enum schema whenever choices belong to one.
- **`"$dictKey"`**: Inside a dictionary value's `TupleMeta`, `InstVarValue = "$dictKey"` resolves to that row's dictionary key. It is reserved and has no meaning outside that row.
- **`InvalidVars`**: Lists fields hidden from the Blueprint property editor and serialiser.
- **`RectRangeVars`**: Associates an `sf.IntRect` with the texture/range field needed by the image-region selector.

```lua
afterBattleVarChanges = {
    type = "Dict[string, Tuple[string, any]]",
    default = {},
    Meta = {
        DictKeyMeta = {
            InstVar = {
                types = { "int", "float" },
            },
        },
        ItemMeta = {
            TupleMeta = {
                [1] = {
                    DropBox = { "=", "+", "-", "*", "/", "//", "%", "**" },
                },
                [2] = {
                    InstVarValue = "$dictKey",
                },
            },
        },
    },
}
```

This field stores as a JSON object whose values are two-element JSON arrays. The key selector lists declared `int` and `float` game variables, tuple element 1 offers an operator drop-down, and element 2 uses the selected variable's numeric editor. These decorators constrain authoring only; the runtime game-variable table does not enforce the declared type.

## Safety rules

Metadata must be pure data: functions, userdata, threads, cycles, non-finite numbers and side effects are forbidden, and defaults must be JSON-convertible. Metadata strings are never evaluated to produce labels; runtime localisation and localised option values are separate concerns.

Core metadata is generated from `BIND_*` annotations — never edit generated Core metadata by hand. See [Generated Metadata and Stubs](<../../Native C++ Development/Generated Metadata and Stubs.md>).
