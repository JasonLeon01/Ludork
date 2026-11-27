# Blueprints, Common Functions and Actors

Create reusable Blueprint classes beneath `Data/Blueprints`, define typed attributes and components, place Actor instances on maps and author visual event graphs.

## Create a Blueprint

Choose **Game → New Blueprint** (or create one from File Explorer), pick a parent class deriving from `BPBase`, and choose a path under `Data/Blueprints`. Parent defaults and events resolve through the full inheritance chain. A new graph-mode Blueprint initialises each inherited event with an `Event → Parent` node chain including parameter links.

A Blueprint key is its case-preserving path relative to `Data/Blueprints` using `/` separators without `.json` (for example `Actors/Guard`), corresponding to the class reference `Data.Blueprints.Actors.Guard`. Key inputs accept an optional `.json` suffix and normalise backslashes and surrounding whitespace. In full class references, every dot separates a module segment (`Data.Blueprints.Actors.json` maps to key `Actors/json`).

## Class and instance values

Class attributes supply defaults for every instance, while map Actors may override individual fields. Metadata-declared components appear as structured fields, and fields listed in `InvalidVars` are hidden from ordinary editing.

The Blueprint editor resolves attributes and components across the entire inheritance chain, giving child overrides precedence over ancestor defaults (see [Actor Components](<Actor Components/Overview.md>)). When a Blueprint's `ID` selects a General Data member, its `attributes` display read-only — edit those values in General Data.

In both the Blueprint editor and **Actor Info**, ordinary variables sit inside grey dashed frames beneath the component list, grouped by the class, Script Mixin or Blueprint that first declared them. Hover over a group heading to view its full class reference; inherited fields stay in their declaring group even when overridden.

### Add a typed attribute

Click **Add Attribute**, enter a name, pick a type and set its initial value. For `enum`, choose an `Enums.*` module in **kind** and select a key (lists and dictionaries also support enum items or values); file fields can restrict the picker with a base directory.

- **Storage and inheritance:** New fields save declarations in `attrDefs` and initial values in `attrs`. Naming an inherited field with no local value locks the type to the inherited declaration and adds an `attrs` override only. Child Blueprints inherit both type and default and cannot redeclare inherited fields; removing an override restores the inherited default. Deleting a locally declared field removes both its declaration and value, and is blocked while any descendant or map Actor override depends on it.
- **Validation:** Every saved attribute must be declared in metadata, an active Script Mixin or inherited/local `attrDefs`. Invalid declarations block loading and saving (reporting Blueprint and attribute). Parent changes and field deletions cannot leave descendants or map overrides incompatible; **Save Invalid Blueprint** permits unfinished graphs but never bypasses attribute errors. See [Blueprint attribute declarations](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md#blueprint-attribute-declarations>).

## Actor placement and Actor Info

Select an Actor Blueprint (deriving from `Engine.Actor`) in any **File Explorer** tab to arm it for placement, then click an empty cell on an editable layer in Actor mode to place an instance (double-click the file to open its editor). Selecting a single placed Actor on the map also arms its Blueprint for placement. Press `Escape` with the map focused to cancel placement. See [Actor previews](<../PreviewHost/UI Actor Particle and Subtitle Previews.md#actor-previews>).

- **Placement persistence:** Switching tabs, navigating history, refreshing, locating resources or selecting non-Actor files does not alter the armed Blueprint. Armed Blueprints follow editor renames, moves and Undo/Redo; deleting the Blueprint or changing its class to a non-Actor cancels placement.
- **Locate Blueprint:** Right-click a map Actor in Actor mode and select **Locate Blueprint** to highlight its file in the most recently active [File Explorer](<Start Page Workspace and Shortcuts.md#file-explorer-and-reference-tree>) (disabled on empty space or Actors lacking a project Blueprint).
- **Actor Info:** Displays the selected Actor's Blueprint reference (with **Open** and **Locate**), grid position `X`/`Y`, tag and exposed fields. Continuous focused position or text edits merge into one Undo step. Click the reset button beside a field to clear its override, or **Reset All** to clear all class-field overrides and restore Blueprint defaults. On hidden layers, **Actor Info** stays inspectable (including Blueprint navigation) while tag, position, reset actions and class fields are read-only.
- **Undo and deletion:** Placing and deleting map instances uses map Undo. Deleting a Blueprint file follows [resource deletion and recovery](<Start Page Workspace and Shortcuts.md#file-explorer-and-reference-tree>): if an instance was deleted and saved before its Blueprint was deleted, map Undo cannot restore it while the Blueprint is missing. Editing Blueprint fields (including Undo/Redo) refreshes Actor visuals from current inherited defaults.

## Graph mode

Enabling **Script Mixin** hides the right-hand graph and preview pane so variables use the full width; disabling it restores the pane and prior column widths immediately while preserving graph data across mode inheritance, Undo/Redo and reloads.

Each graph begins at a red **Event** entry node — connect its execution pin to define the start node (unconnected entries appear translucent with a notice; Blueprints fall back to inherited implementations, whereas Common Function and General Data graphs do not run). See [Execution Flow, Events and Variables](<../Lua and Blueprint Scripting/Blueprint Scripting/Execution Flow Events and Variables.md#events>).

### Events, Parent nodes and drag-to-connect

- **Adding local events:** Click **New Event** to create a local graph (listing inherited events without a local graph; existing local graphs — even empty ones — cannot be added twice). When the parent chain defines the event, the new graph starts with a brown **Parent: Event Name** node wired to the entry and its parameters in one Undo step. Viewing an inherited event tab alone does not create an override or modify saved data.
- **Node actions:** Parent nodes are editable and available only in graph-mode class Blueprints (add via the node picker, or move, copy and delete normally). Right-click a node to disconnect all inputs/outputs, copy or delete it; right-click empty canvas to add, paste or organise nodes. New, pasted and duplicated nodes appear at the pointer (or viewport centre if outside), preserving relative layout and internal links.
- **Drag-to-connect picker:** In any editable Blueprint, Common Function or General Data graph, drag an execution or parameter output pin onto empty canvas to open a pin-filtered picker (execution pins list nodes with execution inputs; parameter pins list nodes with at least one [compatible typed input](<../Lua and Blueprint Scripting/Blueprint Scripting/Execution Flow Events and Variables.md#typed-connections>), filtered by the pin rather than Blueprint class; the right-click picker keeps **Context Sensitive**).
- **Auto-connection rules:** Selecting a node from the dragged picker creates and wires it in one Undo step. Exact parameter type matches take priority over compatible matches (breaking ties by declaration order), and new execution links replace prior ones from that output. Cancelling changes nothing; dropping directly on an input pin connects existing nodes.

### Plain text parameter inputs

Right-click an editable node in any Blueprint, Common Function or General Data graph and choose **Convert to Plain Text Inputs** to edit all unconnected parameters as text boxes (connected inputs keep normal pins); choose **Restore Typed Inputs** to switch back.

- **Type conversion:** String and file inputs preserve text; numbers and booleans parse as literals; containers, vectors and composites use JSON syntax (including `$type`/`$value` for unions); object and function pins keep expression semantics (literal parameters do not gain expression evaluation). See [Metadata Schema and Decorators](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md>).
- **Validation and scope:** Invalid text stays visible as an unsaved draft with an error without overwriting the last valid value and must be fixed before restoring typed inputs or saving (**Save Invalid Blueprint** cannot bypass draft errors; save errors name graph and parameter). Input mode and drafts persist across graph switches within the open window only, clear when closed, are never written to project files or copied, and new/copied nodes default to typed inputs.

### Graph shortcuts

For editor-wide shortcuts and primary modifiers, see [Global shortcuts](<Start Page Workspace and Shortcuts.md#global-shortcuts>).

| Shortcut | Action |
|---|---|
| `Ctrl/Command+N` | Open the node picker at the pointer |
| `Ctrl/Command+A` | Select all editable nodes in the active graph |
| `Ctrl/Command+C` | Copy selected nodes and internal links |
| `Ctrl/Command+V` | Paste at the pointer |
| `Ctrl/Command+D` | Duplicate selection at the pointer |
| `Arrow keys` | Nudge selected editable nodes by one graph unit |
| `Delete` | Delete selected editable nodes |
| `Ctrl+drag` on empty canvas | Box-select enclosed nodes on release |

The virtual entry node is auto-positioned and excluded from select-all, nudging, duplication and deletion (Parent call nodes are editable). Shortcuts yield to focused input fields and mutating actions are disabled in read-only graphs. General Data member graphs expose only the type's events, have no class attributes and disallow latent nodes (see [General Data and Text Config](<General Data and Text Config.md>)).

## Inheritance

A child Blueprint inherits fields, components and behaviour from its parent. Changing a saved parent path can alter inherited fields, events and component compatibility, so verify affected children and map instances carefully. Every Blueprint in an inheritance chain must use either graph mode or Script Mixin mode consistently; see [Script Mixin Authoring Workflow](<../Lua and Blueprint Scripting/Script Mixins/Authoring Workflow.md>).
