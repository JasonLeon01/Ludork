# Script Mixin Authoring Workflow

Use a Script Mixin to implement a Blueprint class's event behaviour in a Lua table instead of a visual graph.

Graph mode and Script Mixin mode are mutually exclusive within one Blueprint inheritance chain. The first Script Mixin Blueprint in the chain provides the script; descendants may inherit it or supply another compatible mixin, but cannot switch back to graph mode.

A Script Mixin Blueprint may omit `graph` because its events execute from Lua. If a `graph` field is present, it must be a structurally valid graph object even though the runtime does not execute it. Graph-mode Blueprints must always store a `graph` object.

## Create and attach a Script Mixin

1. Create or open a Blueprint whose parent provides the lifecycle events and native members the mixin needs.
2. Enable **Script Mixin** on the first Blueprint in the chain.
3. With **Script Path** empty and editable, click **+** beside **...**, choose a directory under `Scripts/Mixins`, and enter a file name (such as `Doors/KeyDoor.lua`). The editor appends `.lua` if omitted.
4. Save to generate the script and its same-named `_meta.lua` companion. The editor selects the new script automatically and hides **+**. To attach an existing script instead, click **...**. Stored paths are relative to `Scripts/Mixins`.
5. Implement the generated event methods and add any editable defaults and metadata fields, then refresh or reopen the Blueprint and save.

- **Naming and file safety**: Script names must start with an ASCII letter or underscore, contain only ASCII letters, digits and underscores, and avoid Lua keywords. Creation never overwrites existing files, and undo/redo restores the selection without deleting created files.
- **Generated template**: The generated script includes every event declared on the resolved Actor type (including inherited events with their most-derived signature), forwarding parameters to `super()` and returning its results. Ordinary methods and `init` are omitted. The companion metadata file declares only the matching type with an empty `attrs` table.

## File pair

`scriptPath` points to an existing `.lua` file relative to `Scripts/Mixins` using forward slashes. Empty segments, `.`, `..`, drive prefixes, absolute roots and `_meta.lua` suffixes are invalid.

```text
Scripts/
└── Mixins/
    └── Doors/
        ├── KeyDoor.lua
        └── KeyDoor_meta.lua
```

The metadata file declares editable fields for the editor and never supplies runtime methods. The runtime script owns behaviour and must not load its metadata companion.

You can also create both files manually following the [Table rules](<Runtime Contract.md#table-rules>) and [Script Mixin Metadata](<Metadata.md>). An empty `attrs` table is valid.

## Inherited scripts

A child Blueprint may omit `scriptPath` to inherit its parent's mixin, or supply another path to layer a compatible child mixin over inherited members. Before replacing a script, verify that field and function kinds match the inherited class and remove any stale saved attributes that no longer belong to the resolved schema.
