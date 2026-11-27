# Declarative UI Authoring Workflow

Declarative UI generates typed Lua Views from JSON assets while keeping business logic in handwritten Controllers. Use the [UI Asset Editor](<../../Editor User Guide/UI Asset Editor.md>) for visual layout.

| Layer | Owns |
|---|---|
| JSON View | Hierarchy, asset-local node names, static control properties, Slots, animation timelines and design-only preview data |
| Generated Lua View | Complete typed control and child-View tree, asset identity, mounting and owned-object lifetime |
| Lua Controller | Model access, callbacks, localised and formatted text, state refresh and dynamic composition |
| Window UI | Native window and input state, its generated View and its private Controller |
| Native control | Rendering, measurement, hit testing, focus and primitive interaction state |
| `UIManager` | Mounting and removing prepared root controls |

> **Important:** Never store Controller functions, expressions, event handlers or `LOC(...)` calls inside a UI asset. Author fixed positions, dimensions, spacing and text styles in JSON; Controllers read prepared controls only when dynamic content requires measurement or resizing.

## File layout

UI assets live under `Data/UI`:

```text
Data/UI/
├── Inventory.json
└── Parts/
    ├── Inventory/ItemRow.json
    └── Shared/KeyPrompt.json
```

Place complete screens, windows, HUDs and overlays directly under `Data/UI`. Place sub-parts owned by one UI under `Parts/<Owner>` and reusable shared parts under `Parts/Shared`. Do not organise directories purely by control type.

Generated Lua Views mirror the asset tree. Everything under `Scripts/Internal/UI`, `Scripts/stub/Internal/UI` and `Scripts/stub/Internal/UIWindows` is generator-owned — do not edit generated files by hand:

```text
Scripts/Internal/UI/
├── Inventory.lua
└── Parts/
    ├── Inventory/ItemRow.lua
    └── Shared/KeyPrompt.lua
```

- **Handwritten UI modules**: Shared foundations live in `Scripts/Internal/UIBase`. Window modules (such as `Scripts/Source/Windows/WindowInventory.lua`) define a private Controller and export the UI class via `Ui.DefineWindow`. Independent row and Scene Controllers live in dedicated `Controller.lua` modules (such as `Source.Windows.WindowInventory.ItemRow.Controller` and `Source.Scenes.SceneTitle.Controller`).
- **Window declarations**: A window's handwritten stub declares only the private Controller using bare `---@meta`; the generator derives the public window declaration under `Scripts/stub/Internal/UIWindows`. Numeric constants outside plain decimal syntax (such as `1e3` or `0x20`) require an explicit `---@field NAME type` annotation on the Controller. See [Declare business members once](<Controller Examples.md#declare-business-members-once>).
- **Generator parsing rules**: The declaration generator resolves top-level `local Name = require(...)` calls from quoted or long-bracket Lua string literals (decoding valid UTF-8 module names) and expects one final `return Ui.DefineWindow(...)`. Malformed strings, long comments or numeric literals report the file, line and column.

## Create the UI and add its behaviour

1. Author the JSON asset in the UI Asset Editor and give every node accessed from Lua a stable name.
2. Enable **Exposed** if other UI assets need to nest it from the Project Palette.
3. Save the asset and click **Export** to generate its View, then `require` that View from your window module.
4. Define a local Controller and return `Ui.DefineWindow(ViewClass, Controller, nativeBase?)`. Place business setup in `init`, native callbacks in `bind`, and state updates in `watch` or `refresh`.
5. Configure initial host behaviour with `windowOptions`, bind authored child panes with `createChild`, and populate variable-length rows with collections. See [Declarative UI Runtime Contract](<Runtime Contract.md>) and [Declarative UI Controller Examples](<Controller Examples.md>).
6. Maintain the private Controller stub and regenerate. Create, mount and dispose of the window from its owner, using [first-use construction](<Runtime Contract.md#create-optional-windows-on-first-use>) for optional popups.

> **Note:** Assets backed by `WindowBase` or `WindowSelectable` nest `Parts/Shared/WindowChrome` and `Parts/Shared/WindowPauseMark` as nodes named `WindowChrome` and `WindowPauseMark`. Their Slots position the chrome over the frame and the pause mark at the bottom content edge; the window base binds return-button, gamepad-hint and pause-mark behaviour automatically.

## Regenerate after asset changes

After moving an asset or renaming nodes, update handwritten `require` paths and node references, then click **Export**. To regenerate UI Views and declarations without a full export, run:

```bash
ludork-assets ui-assets generate <project-root>
```

Ordinary Save and Construct do not regenerate UI code; export before launching or packaging from the command line (see [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>)).

Generation enforces strict file safety:
- Output paths occupied by handwritten Lua files or symbolic links are rejected; only obsolete files carrying the generator marker are removed.
- Updated Views, declarations and case-only directory renames install as one atomic batch. Any installation failure rolls back applied changes (and reports paths if rollback itself fails). A case-only directory rename fails if the directory contains handwritten files.

## Authoring constraints and conventions

- **Screen structure**: Give each visible screen one top-level asset and nest owner-specific sub-panes from `Parts/<TopLevelName>/`. Nested parts require `palette.exposed = true`; runtime-only helpers need not be exposed.
- **Dynamic content and localisation**: Localise tab labels and populate dynamic collections before display; fixed controls and rows stay the instances constructed from JSON. Reserve literal JSON `text` for permanent, language-independent symbols, and use design-only preview fields for placeholders (see [UI previews](<../../PreviewHost/UI Actor Particle and Subtitle Previews.md#ui-previews>)).
- **Animated characters**: Use [`Engine.CharacterView`](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#characterview>) for frame-animated characters and enemies. Provide preview defaults in JSON and call `setCharacter` at runtime with the real Actor visual.
