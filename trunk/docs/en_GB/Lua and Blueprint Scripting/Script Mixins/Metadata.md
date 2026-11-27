# Script Mixin Metadata

A Script Mixin's companion `_meta.lua` file exposes editable fields to the Blueprint editor. It must return `_METADATA` containing exactly one type whose name matches the script file name; it does not declare runtime methods, Blueprint node entries or an inheritance hierarchy.

```lua
local _METADATA = {
    KeyDoor = {
        attrs = {
            "needKeyID",
            "needKeyCount",
        },
        needKeyID = {
            type = { enum = "Enums.GeneralData.Item", valueType = "string" },
            default = "",
        },
        needKeyCount = {
            type = "int",
            default = 1,
        },
    },
}

return _METADATA
```

## Rules

- `attrs` is a dense, ordered array of unique, non-empty field names.
- Every listed field is a table with a valid `type`.
- `default`, when present, is finite, acyclic, JSON-convertible pure data. A field omitted from both saved attributes and `default` is schema-only.
- `bases` is absent or an empty table; Blueprint inheritance supplies the base class.
- The root table contains no second type, function, userdata, coroutine or external-state access.
- Editor decorators refine Inspector controls without changing runtime values; see [Editor names and decorators](<../Blueprint Scripting/Metadata Schema and Decorators.md#editor-names-and-decorators>) for resource and game-variable selectors.

Mixin metadata declares its fields directly for the Blueprint. Store local overrides in `attrs` and do not repeat mixin fields in `attrDefs`. Reserve `attrDefs` for additional Blueprint-defined fields (whose types and defaults descendants inherit). Missing declarations and collisions with inherited or mixin fields fail loading; see [Blueprint attribute declarations](<../Blueprint Scripting/Metadata Schema and Decorators.md#blueprint-attribute-declarations>).
