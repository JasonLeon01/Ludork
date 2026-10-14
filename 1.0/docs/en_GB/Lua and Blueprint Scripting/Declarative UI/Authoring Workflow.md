# Declarative UI Authoring Workflow

Declarative UI in Ludork generates Lua Views from JSON assets and keeps business behaviour in handwritten Controllers. Use the [UI Asset Editor](<../../Editor User Guide/UI Asset Editor.md>) for visual editing.

| Layer | Owns |
|---|---|
| JSON View | Hierarchy, asset-local node names, static control properties, Slots, animation timelines and design-only preview data |
| Generated Lua View | Complete typed control and child-View tree, asset identity, mounting and owned-object lifetime |
| Lua Controller | Model access, callbacks, localised and formatted text, state refresh and dynamic composition |
| Window UI | Native window and input state, its generated View and its private Controller |
| Native control | Rendering, measurement, hit testing, focus and primitive interaction state |
| UIManager | Mounting and removing prepared root controls |

Do not store Controller functions, expressions, event handlers or `LOC(...)` calls in a UI asset.

Author fixed positions, dimensions, spacing and text styles in JSON. Controllers read the prepared controls when content needs measurement or resizing, and leave static layout to the asset. To change a fixed value, edit its asset and regenerate the View.

## Steps

### File layout

The asset root is fixed at `Data/UI/Assets`:

```text
Data/UI/Assets/
    ├── Inventory.json
    └── Parts/
        ├── Inventory/ItemRow.json
        └── Shared/KeyPrompt.json
```

Complete screens, windows, HUDs and overlays live directly below `Assets`. Put parts owned by one large UI in `Parts/<Owner>`, and parts shared by several UIs in `Parts/Shared`. Do not create directories organised only by control shape.

Generated Lua Views mirror the asset hierarchy. Every file under `Scripts/Internal/UI` and `Scripts/stub/Internal/UI` is generator-owned:

```text
Scripts/Internal/UI/
├── Inventory.lua
└── Parts/
    ├── Inventory/ItemRow.lua
    └── Shared/KeyPrompt.lua
```

Handwritten foundations live under `Scripts/Internal/UIBase`. A window module such as `Scripts/Source/Windows/WindowInventory.lua` contains its private Controller and returns the UI class through `Ui.DefineWindow`. Independent row and Scene Controllers use separate modules, such as `Source.Windows.WindowInventory.ItemRow.Controller` and `Source.Scenes.SceneTitle.Controller`.

The matching window stub declares only the private Controller, using bare `---@meta`. Public window declarations are generated under `Scripts/stub/Internal/UIWindows` and must not be edited by hand. See [Declarative UI Controller Examples](<Controller Examples.md#declare-business-members-once>) for the declaration format.

### Create the UI and add its behaviour

1. Create the JSON asset in the UI Asset Editor and give runtime-accessed nodes stable names.
2. Enable **Exposed** to make the asset appear in the Project Palette.
3. Save the asset, then click **Export** to generate its View. Require that View in the window module.
4. Define a local Controller and return `Ui.DefineWindow(ViewClass, Controller, nativeBase?)`. Add business initialisation in `init`, callbacks in `bind`, and state updates through `watch` or `refresh`.
5. Use `windowOptions` for initial window behaviour, `createChild` for authored child panes, and collections for model-dependent rows. See [Declarative UI Runtime Contract](<Runtime Contract.md>) and [Declarative UI Controller Examples](<Controller Examples.md>).
6. Maintain the private Controller declaration and regenerate. Create and mount the window from its owner, then dispose of it when it is no longer needed. For optional popups, keep a separate `Internal.UIBase.LazyWindow` handle and perform complete construction, callback and focus configuration, and mounting in its factory. Use `get()` at the display entry and `peek()` for queries; see [first-use construction](<Runtime Contract.md#create-optional-windows-on-first-use>).

Assets using `WindowBase` or `WindowSelectable` include the shared `Parts/Shared/WindowChrome` and `Parts/Shared/WindowPauseMark` as nodes named `WindowChrome` and `WindowPauseMark`. Their Slots place the chrome over the window frame and the pause mark at the content's bottom edge. These assets own return-button, gamepad-hint and pause-mark layout; the window base binds their behaviour.

### Regenerate after asset changes

After moving an asset or renaming nodes, update handwritten `require` calls and node references, then click **Export**. Alternatively, `ScriptTools ui-assets generate <project-root>` synchronises UI Views and declarations only and removes obsolete generated files. Ordinary Save and Construct do not generate them; export before running or packaging directly from the command line. See [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>) for the full export workflow.

## Limitations

Every visible screen has one top-level asset. Multi-pane screens nest owner-specific assets from `Parts/<TopLevelName>/`. Nested parts use `palette.exposed = true`, while runtime-only helpers may remain unexposed.

Before display, business code localises tab labels and fills dynamic collections. Fixed controls and rows remain the instances created from JSON. Literal `text` is reserved for permanent, language-independent symbols. See [UI previews](<../../PreviewHost/UI Actor Particle and Subtitle Previews.md#ui-previews>) for design-only placeholder content.

Use `Engine.CharacterView` for frame-animated characters and enemies. Assets provide preview values, and the runtime passes the real texture, Rect, scale, animation, material reference and instance parameters to `setCharacter`. Lua does not crop or compose the actor visual.

## Related pages

- [Declarative UI Runtime Contract](<Runtime Contract.md>)
- [Declarative UI Controller Examples](<Controller Examples.md>)
- [UI Asset Schema](<Asset Schema and Control Registry.md>)
- [Script Mixins Overview](<../Script Mixins/Overview.md>)
