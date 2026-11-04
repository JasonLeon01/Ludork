# Script Mixin Authoring Workflow

Use a Script Mixin to define Blueprint behaviour in a Lua table.

Graph mode and Script Mixin mode are mutually exclusive within one Blueprint inheritance chain. The first Script Mixin Blueprint supplies the script. Descendants may inherit it or supply another compatible mixin, but they cannot switch the chain back to graph mode.

A Script Mixin Blueprint may omit `graph`, because its events come from Lua. If a `graph` field is stored, it must still be a structurally valid graph object even though the runtime does not execute it. Graph-mode Blueprints must always store a graph object.

## Steps

1. Create or open a Blueprint whose parent supplies the lifecycle and native members the mixin uses.
2. Enable **Script Mixin** on the first Blueprint in the chain.
3. When **Script Path** is empty and editable, click **+** before **...**. Choose a directory beneath `Scripts/Mixins` and enter a file name, for example `Doors/KeyDoor.lua`. The editor adds `.lua` if no extension is entered.
4. Save to create the script and its same-name `_meta.lua` companion. The editor selects the new script automatically and hides **+**. To attach an existing script, use **...** instead. Stored paths are relative to `Scripts/Mixins`.
5. Edit the generated event methods and add any editable defaults and metadata fields. Refresh or reopen the Blueprint after editing the files, then save.

The script name must start with an ASCII letter or underscore, contain only ASCII letters, digits and underscores, and must not be a Lua keyword. Creation does not overwrite existing files. Undo and redo restore the selection without deleting the created files.

The template contains all Events declared for the resolved Actor type, including inherited Events with the most-derived signature. Each method forwards its parameters to the parent event through `super()` and forwards any return values. Ordinary methods and `init` are not generated. The companion metadata declares only the matching type and an empty `attrs` table; add editable fields as needed.

## File pair

`scriptPath` is an existing `.lua` file relative to `Scripts/Mixins`, using forward slashes. Empty segments, `.`, `..`, drive prefixes, absolute roots and `_meta.lua` suffixes are invalid. Inheritance is described [below](<#inherited-scripts>).

```text
Scripts/
└── Mixins/
    └── Doors/
        ├── KeyDoor.lua
        └── KeyDoor_meta.lua
```

The metadata file is an editor declaration and does not supply runtime methods. The runtime file is the source of behaviour, and it must not load its metadata companion.

You can create the pair manually using the [runtime table contract](<Runtime Contract.md#table-rules>) and [metadata example](<Metadata.md>). An empty `attrs` table is valid.

## Inherited scripts

A child Blueprint may omit `scriptPath` to inherit the current mixin. Supplying another path merges a compatible child mixin over inherited members. Before replacing a script, check that field and function kinds remain compatible, and remove stale saved attributes that no longer belong to the resolved class.

## Related pages

- [Script Mixins Overview](<Overview.md>)
- [Game Script Mixins](<Game Mixins.md>)
