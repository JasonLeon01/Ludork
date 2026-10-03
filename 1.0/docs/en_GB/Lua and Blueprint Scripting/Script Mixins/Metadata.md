# Script Mixin Metadata

The same-name `_meta.lua` file must return `_METADATA` and contain exactly one type whose name matches the script file name. It exposes editable fields for one attached behaviour. It does not declare runtime methods, Blueprint node entries or an inheritance hierarchy.

```lua
local _METADATA = {
    KeyDoor = {
        attrs = {
            "needKeyID",
            "needKeyCount",
        },
        needKeyID = {
            type = "string",
            default = "",
            Meta = {
                GeneralDataVars = "Item",
            },
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
- `default`, when present, is finite, acyclic, JSON-convertible pure data.
- `bases` is absent or an empty table. Blueprint inheritance supplies the base.
- The root contains no second type, function, userdata, coroutine or external-state access.
- Decorators such as `Meta.GeneralDataVars`, `Meta.PathVars` and `Meta.ConfigVars` refine the editor control without changing runtime values. `Meta.InstVar` selects a declared game-variable name, optionally filtered by `types`. `Meta.InstVarValue` names the sibling field whose selected declaration controls the value editor.

Mixin metadata already declares its fields for a Blueprint. Store their local values in `attrs`; do not repeat them in `attrDefs`. Use `attrDefs` only for additional Blueprint fields, whose types and defaults are inherited by descendants. Missing declarations and collisions with inherited or Mixin fields fail loading. See [Blueprint attribute declarations](<../Blueprint Scripting/Metadata Schema and Decorators.md#blueprint-attribute-declarations>).

## Limitations

A field that is omitted from both the saved attributes and `default` is schema-only.

## Related pages

- [Metadata Schema and Decorators](<../Blueprint Scripting/Metadata Schema and Decorators.md>)
