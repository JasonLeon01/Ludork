# General Data and Text Config

General Data is the editor-managed database beneath `Data/General`, suited to enemies, items, equipment, player presets, states and other data shared by many maps or Blueprints. Each JSON file defines a type schema and named members. Its Form, Table and member ability-graph windows share one document and one Undo/Redo history for that type file.

## Type structure

| Key | Meaning |
|---|---|
| `params` | Ordered field definitions with `type`, `defaultValue`, optional `comment`, and recursive type schemas for enums and containers |
| `members` | Named records that store values for those fields |
| `events` | Optional ordered event names; each member may store an independent graph for each one |

Supported fields include strings, booleans, integers, floating-point values, files, lists, dictionaries, `sf.Vector2f/i/u`, `sf.Vector3f/i/u`, `sf.Color` and `sf.IntRect`. New and edited fields save a complete recursive `type`, such as `{ "list": "int" }` or `{ "dict": "int", "key": { "enum": "Enums.GeneralData.Item", "valueType": "string" } }`. Existing `"type": "list"` with `itemType` and `"type": "dict"` with `valueType` remain readable. Enum, tuple and union composition follows [Metadata Schema and Decorators](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md>). Select a union branch before entering its value.

In **Add Field** or **Edit Field**, select `enum`, choose its module through **kind**, and select an inferred or explicit scalar value type and a default value. Managed project enums use `string`. An empty source enum requires an explicit scalar type. For a dictionary, select its key type and key enum independently of its value type. These selectors write the schema itself; no separate `kind` or `reference` property is saved.

Use `{ "enum": "Enums.GeneralData.Item", "valueType": "string" }` for an Item key, `Enums.GeneralDataKey` for a type key, `Enums.Animation` for an animation key, and `Enums.Particle` for a particle key. Selectors include unsaved additions, renames and deletions. Lists, dictionary keys and values, tuples and unions support these enums.

Loading fails on a missing subtype, on a missing or unknown non-underscore member field, and on any value that does not match the schema. Use `any` for heterogeneous JSON values, because it preserves literal types and never evaluates strings.

`linkedType` is unsupported and rejected.

Each field definition may store an optional English `comment`. Enter it through **Comment** when adding or editing a field, and an empty comment is omitted. Form field names and Table headers show the comment on hover. Comments are plain project text, not locale keys or member data.

String enum selectors include **— None —**, stored as an empty string. Unknown nonempty values are retained; enums validate scalar types rather than option membership. Empty managed collections remain valid string enums. An enum-keyed dictionary row remains pending until a nonempty, unique key is chosen, so no placeholder key is saved. The old `GeneralDataVars` decorator and General Data field `reference` protocol are unsupported; express these choices in the type schema.

Enum declarations count as dependencies even when no member selects a value. Enum IDs preserve whitespace and quote characters literally.

Renaming a type updates recognised module paths in editable JSON, including self references and module names reassigned by identifier collisions. Lua metadata dependencies remain visible, but their source is not rewritten.

**Change ID** updates typed member references in defaults, Blueprint and map fields, graph parameters and containers, including dictionary keys. Inherited values change in their owning definition; child Blueprints and map Actors receive no overrides. Animation and particle renames update typed scalar references. Dictionary key collisions reject the whole rename. Undo/Redo follows the shared [reference-update rules](<Start Page Workspace and Shortcuts.md#editing-and-saving>). Ordinary strings and Lua or C++ source require manual updates. Remove and save dependencies before deleting a referenced type.

## Steps

1. Open **Database → General Data** (`F10`).
2. Select a type, or right-click the type-tab strip to create one.
3. Define fields before creating many members. Right-click a Form field name to edit its definition.
4. Add members and fill their values. Right-click a member in the list or a Table row to change its ID, duplicate it, remove it or open an available ability graph.
5. Manage type events from the type tab's context menu, then open member graphs with **Edit Ability Graph**. Event renames and deletions update all member graphs, and the runtime activates the graphs explicitly.
6. Inspect references before renaming or deleting types and members.
7. Save the data.

Save generates `Enums.GeneralDataKey`, each `Enums.GeneralData.<TypeName>` module, `Enums.Animation`, `Enums.Particle`, and `Source.Configs.GeneralDataTypes` with its LuaLS declaration. For example, `local Item = require("Enums.GeneralData.Item")` exposes `Item.EnemyBook`. Edit database records rather than these generated files. Type renames and deletions update their generated modules; handwritten files are retained.

Use `GeneralDataTypes.Create(typeName, memberID, memberData)` to restore a stored member with its ID and field types.

Editing a field can change its name, type, container item or value type, default and comment. Renaming preserves each member's current field value. Changing the field type, a list's item type or a dictionary's key or value type first asks for confirmation, then resets that field in every existing member to the new default value. Changing only the default does not overwrite existing members. Removing a field removes its meaning but does not automatically repair the scripts that read it.

Member graphs allow only declared events and synchronous nodes, with no class inheritance, Parent or self node. Use global nodes and the `GameplayEventData` context. Activation and results are described in [General Data event execution](<../Lua and Blueprint Scripting/Blueprint Scripting/Execution Flow Events and Variables.md#events>).

## Search and views

Search matches member IDs and direct scalar values such as names, numbers and booleans, ignoring case. It does not search inside containers or graphs. Each type retains its search, selection and Form/Table view while the window remains open.

Form edits one member in full. Table shows the ID and schema fields in declaration order, with direct editing for booleans, numbers and strings. For structured values, choose **Edit** to open that member in Form. Columns scroll horizontally. Editing follows the shared [Undo rules](<Start Page Workspace and Shortcuts.md#editing-and-saving>).

![General Data editor](../../../images/editor/general-data.png)

## Game Variable Manager

Open **Database → Game Variables** (`F9`) to declare mutable values shared by Blueprint and the active game instance. The left side lists the declared variable names. Select one to edit its type, initial value and optional remark on the right, and the remark remains editor-only with no runtime effect. Variable names are runtime identifiers, so keep them stable once graphs, maps or scripts begin to reference them.

Use the search box to filter variable names case-insensitively. **New Variable** adds a declaration after asking for its name and type, and **Delete Variable** removes the selected declaration after confirmation.

![Game Variable Manager with a selected integer variable](../../../images/editor/game-variables.png)

Choose the type when creating a variable. The manager offers `bool`, `int`, `float`, `string`, `list` and `dict`, and these are written to metadata as `bool`, `int`, `float`, `string`, `any[]` and `Dict[string, any]` respectively. Lists and dictionaries use ordinary Lua tables, not the native `list` or `dict` of Standard. Changing an existing variable's type clears its initial value and replaces it with the empty default of the new type.

Save generates `Scripts/Source/Configs/GameVariables.lua` and `GameVariables_meta.lua` together. The first holds initial values and the second holds declarations. Edit variables through the manager, not these files; opening the manager alone writes nothing.

Blueprint fields or node parameters marked with `InstVar` use a searchable selector instead of a free-form text box. A corresponding `InstVarValue` field reads the type of the selected declaration and switches to the normal editor for that type. A type-filtered selector, such as the Game project conditional door, lists compatible variables only.

## Text Config

Ordinary UI text is styled on its UI node. Use **Game → New Text Config** for shared styles or named rich-text tags.

![Text Config editor](../../../images/editor/text-config.png)

Display strings such as `{ITEM_NAME}` are resolved when presented, not when data is loaded. See [Game Localisation Workflow](<../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>) for authoring and using localised text.

See [General Data Reference](<General Data Reference/Overview.md>) for the Game schemas.

## Limitations

Changing a schema or a game-variable declaration cannot automatically repair scripts, graphs, saves or string references. General Data and the generated game-variable declarations are project definition data, not stores for mutable play-session state.

## Related pages

- [Class](<General Data Reference/Class.md>) — the Game project Class schema
- [Blueprints, Common Functions and Actors](<Blueprints Common Functions and Actors.md>)
