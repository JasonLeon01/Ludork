# Blueprints, Common Functions and Actors

A Blueprint is a JSON class definition beneath `Data/Blueprints`. It selects a native or Lua parent, stores editable attributes and, in graph mode, contains event graphs. Actor instances placed on maps refer to these classes.

## Create a Blueprint

Use **Game → New Blueprint**, or the equivalent command in the file explorer. Choose a parent that ultimately inherits `BPBase`, then choose a path beneath `Data/Blueprints`. Parent defaults and supported events are resolved from the inheritance chain. A new graph-mode Blueprint starts each inherited event with `Event → Parent`, including the event parameter connections.

A Blueprint key is its case-preserving path relative to `Data/Blueprints`, with `/` separators and no `.json` extension, such as `Actors/Guard`. Its class reference is `Data.Blueprints.Actors.Guard`. Key inputs accept an optional `.json` extension and normalise backslashes and surrounding whitespace. In a full class reference, every dot separates a module segment: `Data.Blueprints.Actors.json` refers to the key `Actors/json`.

## Class and instance values

Class attributes provide defaults for every instance, and a map Actor may override individual values. Components are declared by metadata and appear as structured fields. Hidden fields from `InvalidVars` are not ordinary editable properties.

The Blueprint editor resolves variable and component declarations through the complete inheritance chain. A child’s explicit value takes precedence over inherited defaults. See [Actor Components](<Actor Components/Overview.md>) for available components, adding and editing them, and instance overrides.

In the Blueprint editor and **Actor Info**, ordinary variables are grouped in grey dashed frames beneath the component list. Each frame names the class that first declared its fields, and hovering over the grey heading shows the full class reference. Inherited fields appear only in their original group, even when a child overrides their values. Fields contributed by a Script Mixin and custom Blueprint fields are attributed to the first Script Mixin or Blueprint that declares them.

When a Blueprint’s `ID` selects a General Data member, its `attributes` are shown read-only. Edit those values in General Data.

### Add a typed attribute

Use **Add Attribute** to enter a name, choose its type and set the initial value. For `enum`, choose an existing `Enums.*` module in **kind**, then select a key for the value. Lists and dictionaries also support enum items or values. A file field can constrain the asset selector with a base directory.

A new field saves its declaration in `attrDefs` and its initial value in `attrs`. If the name already belongs to an inherited field with no local value, the dialog fixes the type to that declaration and adds only an `attrs` override. Child Blueprints inherit the type and default; they cannot redeclare the inherited field. Removing an inherited override restores its inherited default. Deleting a field introduced by the current Blueprint removes both its declaration and local value, and is rejected while a descendant or map Actor override still depends on it.

Every saved attribute needs a declaration in metadata, an active Script Mixin or inherited/local `attrDefs`. An invalid declaration prevents the Blueprint from loading or being saved; the error names the Blueprint and attribute. Parent changes and field removal cannot leave descendants or map overrides with incompatible declarations. **Save Invalid Blueprint** permits unfinished graphs, but cannot bypass attribute errors. See [Blueprint attribute declarations](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md#blueprint-attribute-declarations>) for the JSON format.

## Actor Library and Actor Info

The **Actor Library** lists Actor Blueprints in icon or list view. Select an entry for placement, or double-click it to open the Blueprint. Visual checks are described in [Actor previews](<../PreviewHost/UI Actor Particle and Subtitle Previews.md#actor-previews>).

Filter by top-level folder and **All**, **Favourites** or **Recent** (the last 20 selected or placed Actors). Search matches names or Blueprint paths across all Actors, ignoring case; category and scope filters are disabled until the search is cleared.

Right-click an entry to open or locate its Blueprint, toggle its favourite state or remove it from Recent. Favourites are stored in `Main.proj` under `editor.actorFavorites`; they follow file moves and renames and are removed when the Blueprint is deleted.

In Actor mode, right-click an Actor on the map and choose **Locate Blueprint** to switch to File Explorer and select its Blueprint file. The action is disabled on empty space and for Actors without a Blueprint in the project.

For a selected map Actor, **Actor Info** shows its Blueprint reference with the **Open** and **Locate** actions, the grid position X/Y, the tag and the exposed class fields. Position and text edits merge into one Undo step for a continuous focused edit. A reset button beside an overridden field removes that instance override, and **Reset All** removes every class-field override for the instance and reveals the inherited Blueprint defaults again. When the Actor's layer is hidden, **Actor Info** stays available for inspection and Blueprint navigation, but its tag, position, reset actions and class fields are read-only.

Placing and deleting Actor instances can be undone in their map document. Deleting the Blueprint file itself is immediate and cannot be undone. If an instance was deleted and saved before its Blueprint was deleted, Undo cannot bring that instance back while the Blueprint is missing. Blueprint field changes, including Undo and Redo, refresh Actor visuals using the current inherited defaults.

## Graph mode

Enabling **Script Mixin** hides the entire right-hand graph and preview area, and the variables fill the available width. Disabling it restores that area and the previous column widths immediately. The same layout follows the inherited mode, Undo/Redo and reload, and existing graph data is retained.

Each graph has a red **Event** entry. Connect its execution output to begin the graph; an unconnected entry is translucent and shows a notice. A Blueprint can fall back to an inherited implementation, while Common Function and General Data graphs do not execute. Event parameters and Parent calls are explained in [Execution Flow, Events and Variables](<../Lua and Blueprint Scripting/Blueprint Scripting/Execution Flow Events and Variables.md#events>).

Use **New Event** to add a local graph. The name input offers inherited events that have no local graph; an existing local graph, even an empty one, cannot be added again. If the parent chain has the event, the new graph starts with a brown **Parent: Event Name** node connected to the entry and its parameters. This is one Undo operation. Opening an inherited event tab only displays it and does not initialise an override or modify saved data.

Parent nodes are editable and available only in graph-mode class Blueprints. Add them from the node picker, or move, copy and delete them like other nodes. Right-click a node to disconnect all inputs or outputs, copy or delete it; right-click empty canvas to add, paste or organise nodes.

Right-click an editable node and choose **Convert to Plain Text Inputs** to replace all its editable parameter controls with text boxes. Connected inputs retain their usual display and pin types. Use **Restore Typed Inputs** to return to the type-specific controls. This works in Blueprint, Common Function and General Data graphs.

Plain text inputs still convert values to the parameter's declared type. String and file values preserve text; numeric and Boolean values become their corresponding literals. Containers, vectors and composite values use their existing JSON shape, including `$type`/`$value` for unions. Object and function inputs retain their existing expression semantics; this mode does not add expression support to numeric or other literal parameters. See [Metadata Schema and Decorators](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md>) for stored shapes.

Invalid text remains visible as an unsaved draft with an error and does not replace the last valid parameter value. Correct it before restoring typed inputs or saving; **Save Invalid Blueprint** cannot bypass a draft error. Save errors identify the graph and parameter. The input mode and drafts survive graph switches and control refreshes only within the current editor window. Closing that window clears them; they are not written to the project or copied with nodes. New and copied nodes use typed inputs.

New, pasted and duplicated nodes are placed at the pointer, or at the viewport centre if the pointer is outside the graph. Duplicating a selection preserves its relative layout and internal links.

The primary modifier is `Ctrl` on Windows and `Command` on macOS.

| Shortcut | Action |
|---|---|
| `Ctrl/Command+N` | Open the node picker at the pointer position |
| `Ctrl/Command+A` | Select all editable nodes in the current graph |
| `Ctrl/Command+C` | Copy the selected nodes and their internal links |
| `Ctrl/Command+V` | Paste at the pointer position |
| `Ctrl/Command+D` | Duplicate the selection at the pointer position |
| `Arrow keys` | Move selected editable nodes by one graph unit |
| `Delete` | Delete selected editable nodes |
| `Ctrl+drag` on empty canvas | Box-select nodes; drag further to enclose more nodes, release to apply |

The built-in entry is virtual and automatically positioned, so it is excluded from select-all, manual movement, duplication and deletion. Parent call nodes remain editable. Graph shortcuts do not replace keyboard input while a parameter editor or another input control has focus, and mutating shortcuts are disabled for read-only graphs.

General Data member graphs use the type’s event list, have no class attributes and cannot contain latent nodes. Their runtime behaviour is described in [General Data and Text Config](<General Data and Text Config.md>).

## Inheritance

A child Blueprint inherits fields and behaviour. Changing a saved parent path can alter inherited fields, events and compatible component data, so treat such a change with care. A Blueprint inheritance chain must use either graph mode or Script Mixin mode throughout; see [Script Mixin Authoring Workflow](<../Lua and Blueprint Scripting/Script Mixins/Authoring Workflow.md>).

## Related pages

- [Tilesets, Autotiles and Maps](<Tilesets Autotiles and Maps.md>)
