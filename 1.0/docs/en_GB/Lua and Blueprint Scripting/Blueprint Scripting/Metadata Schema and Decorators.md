# Metadata Schema and Decorators

`_METADATA` is the pure-data schema for Blueprint fields, signatures, editor controls and execution protocols. It tells the editor which Lua and C++ members can be edited or used as Blueprint nodes. A metadata file runs in an isolated Lua state and must finish with a pure-data table named `_METADATA`.

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

Every type table contains ordered `attrs`. Do not rely on Lua table key order. A field requires `type`. It appears with an ordinary value only when the Blueprint already stores it or metadata supplies a `default`.

Primitive names include `bool`, `int`, `float`, `string`, `function`, `event`, `any` and `Pair`. SFML values use the `sf.` prefix. Core types use `{ "Module", "Type" }`, with `[]` on the second item for arrays.

Container types compose recursively. `T[]` is a variable-length list of one element type, `Dict[string, T]` is a dictionary with string keys, and `Tuple[T1, T2, ...]` is a fixed-length heterogeneous array. Examples include `Dict[string, Tuple[string, any]]`, `Dict[string, int[]]` and `int[][]`.

The capitalised `Dict[...]` and `Tuple[...]` tokens are metadata schema descriptions, not constructors for Standard's lower-case runtime `dict` and `tuple` containers. Metadata defaults must be JSON-convertible pure data. Container defaults use plain Lua tables, not native `list`, `tuple` or `dict` values. Runtime node pins that accept native containers must declare their qualified types, such as `{ "_G", "list" }`, or include them in a union. A `T[]` pin accepts a Lua array. Use an explicit conversion node to pass a native list to it.

A `string` field preserves literal text, including `"nil"`. Whole-value expressions are evaluated once, so `nil` in an `any` expression produces Lua nil. Strings nested inside saved containers remain literal.

Unset node inputs are saved as JSON `null` and use the declared default when present. JSON-value editors accept `null` directly. Clear a function expression to leave it unset. An `any` input is edited as text: when it loses focus, text that parses as a JSON literal, such as a number, is stored as that literal, and other text is stored as an expression string.

Node **Convert to Plain Text Inputs** bypasses editor selectors and numeric controls while preserving this schema. `string` and `file` values preserve all text, including whitespace and `null`; `int` requires a signed 64-bit integer without a fractional part, and `float`, `double` and `number` require finite numbers with an invariant decimal point. Boolean input accepts `true` or `false`. Blank input is unset except for `string`, `file` and `any`, which preserve it as text. Containers and composite values require valid JSON: vectors and `Pair` use component arrays, `sf.Color` uses four integer channels from 0 to 255, `sf.FloatRect` uses four numbers, and `sf.IntRect` uses a nested array such as `[[0, 0, 32, 32]]`. Unions retain the declared `$type`/`$value` wrapper, including within containers.

Plain `any` input retains the existing JSON-or-expression conversion, including preserving blank text and the original text of a quoted JSON string; enter `null` to leave it unset. Function/event expressions and object references keep their existing runtime semantics. Invalid text is an editor-session draft, never a saved parameter value; it blocks saving and returning to typed inputs until corrected. The mode and drafts are window-local and do not change metadata or Blueprint JSON.

The declared schema takes priority over the current Lua table or JSON value shape, including for empty containers. A `Dict` serialises as a JSON object. A list or `Tuple` serialises as a JSON array. Metadata with a known shape must use an explicit recursive type.

Runtime calls must supply the declared types. Conversion rules are in [Runtime Value Boundaries](<../../Native C++ Development/Runtime Value Boundaries.md>).

`component = true` marks component fields. Composite controls are selected from their real type: vectors, colour, rectangles and `Pair`. `Pair` is a two-component numeric control type, not a fixed heterogeneous `Tuple`. `RectRangeVars` adds texture/range semantics to `sf.IntRect`.

## Blueprint attribute declarations

A Blueprint JSON document stores values in `attrs` and declares fields introduced by that Blueprint in the sibling `attrDefs` object. Omit `attrDefs` when the class introduces no fields. Each definition requires `type`, using the same primitive, qualified, enum or recursive schema as metadata. `file` alone may also declare `base` to constrain its asset selector; the value still stores a complete `/Game/Assets/...` path.

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

This excerpt shows the declaration and value portions of a Blueprint. A definition contains only `type` and optional file `base`; values and initial defaults belong in `attrs`. A declaration may remain without a value. The editor and runtime retain its exact type rather than inferring it from a number, string or empty container.

Resolve declarations from Lua/native metadata, the active Script Mixin and ancestor Blueprints before adding local `attrDefs`. A child inherits both type and default: it may put a new value in `attrs`, but must not repeat or change the inherited definition in `attrDefs`. Local names must not collide with existing fields, methods or reserved runtime names. For example, an inherited `speed` or Mixin `needKeyCount` is a value override, not a new declaration.

Every saved attribute and nested typed record member must be declared, and its value must match that schema. Unknown fields, missing definitions, invalid schemas and conflicting declarations fail loading with the Blueprint path and field. The editor and runtime do not infer missing declarations or convert an older untyped shape. Map `BPClassVarChanged` entries also store values only and use the Actor class's inherited schema. Graph-only Common Functions and General Data member graphs have no class declarations.

## Enum schemas

An enum references one module under `Scripts/Enums`. The source contains a table of named scalar literals and its EmmyLua annotation:

```lua
-- Scripts/Enums/StairDirection.lua
---@enum Enums.StairDirection
local StairDirection = {
    None = "None",
    Up = "Up",
    Down = "Down",
}

return StairDirection
```

Use its require path in the recursive schema:

```lua
stairDirection = {
    type = { enum = "Enums.StairDirection" },
    default = "None",
}
```

The same `{ enum = "Enums.StairDirection" }` schema works directly in a function parameter or return entry and inside `list`, `dict`, `tuple` or `union`. Each enum keeps its type annotation in its source file, without a mirrored stub.

The static reader accepts either a directly returned literal table or one local literal table followed by `return` of the same variable. Comments, annotations and optional semicolons are allowed, as is one pair of parentheses around a scalar literal for an inline type annotation. Calls (including `require`), calculations, table mutations, other returned variables and extra statements are rejected. The editor never executes the module.

Keys must be nonempty strings, and values must share one category: strings, booleans or finite numbers. Without `valueType`, an enum containing only integer literals is `int`; any floating-point literal, including `1.0`, makes it `float`.

The optional `valueType` declares `string`, `bool`, `int` or `float`, for example `{ enum = "Enums.GeneralData.Item", valueType = "string" }`. It allows an empty constant table and fixes the type before any options exist. Nonempty constants must agree with it; integer constants may also use an explicit `float` type. An empty source module without `valueType`, an incompatible constant, an unknown schema property or a missing module is an error, including inside a nested container. `valueType` describes the scalar type, not membership in the option list.

The selector displays keys in ordinal order and saves the selected value itself. It stores neither the key nor a Lua expression, and needs no `Meta.DropBox`. Equal values select the first matching key for display. Existing values absent from the current table remain saved and show an unknown-value message. They remain valid if their underlying scalar type is valid; an enum does not impose runtime membership checks.

An explicit default is the actual scalar value and takes priority. Without one, new node inputs and container items use the first option, or the declared scalar's default when the enum is empty (`""`, `false` or `0`). An attribute without a default remains schema-only until it has a saved value. String enum selectors include **— None —**, which stores `""`; unknown nonempty values remain visible and preserved.

For ordinary enums, the editor reads the current file whenever the field is displayed or its selector opens, including after returning to the window. It does not execute gameplay modules. Project-managed enums instead use the current project catalog, including unsaved data: `Enums.GeneralDataKey` selects General Data types, `Enums.GeneralData.<TypeName>` selects their members, `Enums.Animation` selects animation keys, and `Enums.Particle` selects particle keys. All four use `valueType = "string"`; their selectors work before source generation. A deleted managed module is an error even if an old generated file remains on disk. An invalid enum reports its module path and preserves the current value.

Dictionary schemas accept a string enum key independently of their value schema:

```lua
type = {
    dict = "int",
    key = { enum = "Enums.GeneralData.Item", valueType = "string" },
}
```

Omitting `key` keeps ordinary string keys. Explicit keys must be `string` or an enum declaring `valueType = "string"`; numeric, boolean, container and untyped enum keys are rejected. Values retain their own recursive type. JSON remains an object with the selected scalar strings as keys. Adding an enum-keyed row waits for a nonempty unique key before changing saved data; duplicate keys are rejected. `DictKeyMeta` continues to refine unrelated key controls without replacing the declared key type.

**Convert to Plain Text Inputs** edits the underlying scalar value using its ordinary `string`, `bool`, `int` or `float` rules. For an integer enum, enter the number rather than the displayed key. Enum identity remains in the schema, including a union's `$type` branch descriptor; the stored value is still the scalar. [Typed connections](<Execution Flow Events and Variables.md#typed-connections>) use that underlying type as well.

Native enums and marked constant maps generate equivalent modules such as `Enums.Engine.Direction`, `Enums.GlobalCore.WeatherType` and `Enums.sf.Keyboard.Scan`. Their generation and ownership are described in [Generated Metadata and Stubs](<../../Native C++ Development/Generated Metadata and Stubs.md#generated-enum-modules>).

## Union schemas and stored literals

A union lists its allowed branches as `{ union = { T1, T2 } }` and appears in the editor as `Union[T1, T2]`. It composes with `{ list = T }`, `{ dict = T }` and `{ tuple = { T1, T2 } }`, the structured equivalents of `T[]`, `Dict[string, T]` and `Tuple[...]`.

A declared default determines the initial branch and value. Otherwise, a new node selects the first branch with a valid literal default. Branches without one remain unset. Switching branches resets the value to the selected branch's default.

```lua
type = { union = { "sf.Vector2f", "sf.Vector2u", "nil" } },
default = { ["$type"] = "sf.Vector2u", ["$value"] = { 64, 32 } },
```

This saves as `{ "$type": "sf.Vector2u", "$value": [64, 32] }`. `$type` identifies the branch with its qualified name or recursive container schema, such as `{ "list": "sf.Vector2u" }`, and so distinguishes types with the same stored shape.

Explicit nil is `{ "$type": "nil", "$value": null }` in JSON and `{ ["$type"] = "nil" }` in Lua metadata. It remains distinct from an unset value, which uses ordinary defaults.

Use this wrapper only where the schema declares a union, including nested unions. Runtime calls receive the restored value, without the wrapper. Invalid values report the affected field or node parameter.

## Inheritance

`bases` is an ordered array of direct metadata bases, each written as `{ "Module", "Type" }`. Include only bases that participate in editor field inheritance. Script Mixin metadata cannot declare bases.

When a Lua module directly returns its class, declare `moduleReturn = true` inside that class's metadata table. The metadata file must contain exactly one type. Class selectors and new Blueprint parents then use the module path, such as `Source.MapActors.Enemy`. A class exposed as a named module member keeps its full reference, such as `Engine.Actor`, and omits this flag. Metadata type references, including `bases`, fields and node pins, still use the explicit module/type pair, such as `{ "Source.MapActors.Enemy", "Enemy" }`.

## Functions and events

`parameters` and `["return"]` contain an ordered name array plus name-to-type entries. The optional `default` array aligns with parameters. `Pure` is explicit. `ExecSplit`, `Latent`, `LoopNode` and event declarations describe execution protocol consumed by the graph runtime.

Every ordinary instance method declares `"self"` as its first parameter, with the owning class's full module/type reference and `default[1] = "self"`. The first example describes an instance method of the class returned by `Source.SoundFilter`. Its receiver pin remains visible in every Blueprint context. Connect an instance to call another object; an unconnected, unset receiver uses the current graph owner. A missing owner, an incompatible owner, or an explicitly connected nil receiver is an error. Static functions and event entries do not declare a receiver parameter. Lua colon-call signatures and native method declarations keep their usual argument lists.

Receiver and business arguments share one ordered parameter array. Include the receiver when positioning defaults and saved parameter values; metadata type names remain explicit even when `moduleReturn = true`. For example, a SceneMap receiver is `{ "Source.Scenes.SceneMap", "Scene" }`.

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

The array part fixes pin order. The string keys fix pin types. Each parameter default occupies the same position as its parameter, and `nil` means no default. A `nil` entry named in `["return"]` is still an output pin, whereas an empty table means no outputs. `Pure = true` alone removes execution pins.

Use `defaultUnset = { "parameterName" }` for a parameter whose initial value must remain nil instead of receiving an editor-generated literal. For example, `Scene.recordActorPosition` declares `position: sf.Vector2i` and `defaultUnset = { "position" }`, preserving the API's default of recording the Actor's current position. Parameter names in `defaultUnset` are independent of receiver-related index shifts.

## Execution declarations

- `ExecSplit = { "success", "fail", success = true, fail = false }` orders execution outputs and maps them to runtime result values.
- `Latent = true` marks suspension; ordered `LatentStates` maps resume outputs to stage values. Declare the return type as `{ "Engine", "AsyncOperation" }`; see [execution flow](<Execution Flow Events and Variables.md#execution-semantics>) for runtime behaviour.
- `Loop = true` marks a loop node; `LoopNode = "ForEach"` or `"ForLoop"` selects the runtime protocol.
- `type = "event"` creates an event entry only. Its parameters and returns use the same signature format. Without an explicit split, the event receives `ExecSplit = { "default", default = "nil" }`; the string `"nil"` is the metadata token for the default branch.

These declarations are promises to the Blueprint runtime. The Lua or C++ implementation must actually return split values, resume latent states or perform the declared iteration.

## Editor names and decorators

The editor derives ordinary type, field, function, event, pin and Blueprint-variable labels from their identifiers. It splits camel case, Pascal case and underscore boundaries, then capitalises each displayed word. For example, `textureRect` is displayed as `Texture Rect`. This changes presentation only. Metadata keys, runtime member names and serialised Blueprint names remain unchanged.

Do not declare `DisplayName`, `DisplayDesc`, `VariableDisplayNames`, `VariableDisplayDescs`, `ParameterDisplayNames` or `ParameterDisplayDescs` for ordinary members. The editor does not evaluate `LOC(...)` or any other expression to obtain schema labels or default descriptions.

Operator nodes such as `ADD` and `IADD` are the exception. They may use a direct literal `Meta.DisplayName`, such as `+` or `+=`, when identifier formatting cannot express the intended label. Do not wrap the literal in `LOC(...)`, and do not add a default `DisplayDesc`.

`Meta` retains editor semantics that cannot be inferred from the declared type, such as `DropBox`, `PathVars`, `ProgressVars`, `SliderVars`, `RangeVars`, `MoveRouteVars`, `Transfer`, `BlueprintClassVars`, `CommonFunctionVars` and `ConfigVars`.

For game resources, `PathVars` supplies a full selector root such as `/Game/Assets/Sounds`, and the field stores the complete logical path. `PathRoot = "Project"` instead selects native project files such as Script Mixins. `PathRoot = "Data"` selects data files beneath a project-relative `PathVars` directory such as `Data/Subtitles`, and stores the full project-relative `Data/...` path. Use `PathFilter = "*.json"` for JSON documents. Data references must remain within the selector directory and cannot contain traversal segments. Function metadata targets a parameter by name, for example `PathRoot = { subtitleFileName = "Data" }`.

`InstVar` marks a string field or parameter as a game-variable name. The editor replaces free-form text entry with a searchable selector that uses current game-variable declarations, including unsaved edits. A direct field uses `InstVar = true`, or `InstVar = { types = { "int", "float" } }` to keep only declarations with one of the listed metadata types. A function metadata table projects the decorator to named parameters with `InstVar = { "valueName" }`.

`InstVarValue` links another field or parameter to that selected name. A direct field writes the sibling name as a string, for example `InstVarValue = "openConditionName"`. A function projects parameter links as a table, for example `InstVarValue = { value = "valueName" }`. Once the name is selected, the linked value uses the ordinary editor for the declaration's type. The decorator changes editor selection and input controls only. Runtime reads and writes still use the actual game-variable implementation.

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

Container metadata refines editors at the immediate container layer without changing the declared schema or serialised shape. `DictKeyMeta` applies its editor decorators to each string key. `ItemMeta` applies to each immediate list item or dictionary value. When that item is a `Tuple`, `ItemMeta.TupleMeta` maps 1-based tuple positions to their own decorators. `DropBox` provides ordered literal choices for a drop-down when there is no shared enum module; use an enum schema when the choices already belong to one. Within a dictionary value's `TupleMeta`, the reserved `InstVarValue = "$dictKey"` reference resolves to that row's dictionary key. It is not a field name and has no meaning outside that row.

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

This example is still stored as a JSON object whose values are two-element JSON arrays. The key selector lists only declared `int` and `float` game variables, the first tuple item is the operator drop-down, and the second uses the selected key's numeric editor. The metadata constrains authoring only. The runtime game-variable table does not enforce the declared type.

`InvalidVars` lists fields hidden from the ordinary Blueprint property editor and serialiser. `RectRangeVars` associates an `sf.IntRect` with the texture/range field required by the image-region selector. Decorator names and their ordered data are preserved. Do not invent wrapper structures such as `fields`, `nodes` or `events`.

## Safety rules

Metadata cannot contain functions, userdata, threads, cycles, infinite numbers or side effects. Defaults must be JSON-convertible pure data. Metadata strings are data, and the editor never evaluates them to derive member or node labels. Runtime localisation and localised option values are separate concerns.

Core metadata is generated from `BIND_*` annotations. Never edit generated Core metadata by hand. See [Generated Metadata and Stubs](<../../Native C++ Development/Generated Metadata and Stubs.md>).

## Related pages

- [Script Mixin Metadata](<../Script Mixins/Metadata.md>)
- [Generated Metadata and Stubs](<../../Native C++ Development/Generated Metadata and Stubs.md>)
