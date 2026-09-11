---
name: ludork-ui
description: Create or change Ludork Avalonia form inputs, declarative UI assets/controllers and native UI adapters. Use for input styling, UI asset moves, ownership and cross-layer UI contracts.
---

# Ludork UI

Choose the relevant section; an editor form change does not need the declarative runtime documentation.

## Editor form inputs

Use [Editor/Views/Utils/EditorInputs.cs](../../../Editor/Views/Utils/EditorInputs.cs) as the styling source of truth, matching Map Edit:

| Control | Factory or application API |
|---|---|
| Editable text | `CreateEditableTextBox()` / `ApplyEditable(textBox)` |
| Numeric input | `CreateNumericUpDown(...)` / `ApplyEditable(numeric)` |
| Browse-selected path or read-only display input | `CreateReadOnlyTextBox()` / `ApplyReadOnly(textBox)` |

Call these on `EditorInputs`; do not recreate their colours, borders, padding or focus behaviour. Read-only inputs must remain non-focusable, outside the tab order and free of an editable caret. Pure multiline display such as Markdown code blocks is exempt. Colour-picker numeric inputs may use `stretch: false` with a dedicated width.

## Declarative assets and controllers

Read [Declarative UI Authoring Workflow](<../../../docs/en_GB/03.Lua and Blueprint Scripting/04.Declarative UI/01.Authoring Workflow.md>) and the affected [Asset Schema](<../../../docs/en_GB/03.Lua and Blueprint Scripting/04.Declarative UI/04.Asset Schema and Control Registry.md>) or [Runtime Contract](<../../../docs/en_GB/03.Lua and Blueprint Scripting/04.Declarative UI/02.Runtime Contract.md>) section before editing. For native adapters, read [Native UI Adapters](<../../../docs/en_GB/04.Native C++ Development/08.Native UI Adapters.md>).

- Complete UIs live directly under `Data/UI/Assets`; internal parts belong in `Parts/<LargeUiName>` and shared parts in `Parts/Shared`. `Scripts/Source/UI` and `Scripts/stub/Source/UI` are fully generated asset mirrors; `Scripts/stub/Source/UIWindows` contains generated public window declarations. Never hand-edit these outputs. Editor Export owns their generation alongside Export plug-in hooks; ordinary Save, Construct, direct run/pack and template creation do not generate them. The three generated directories are ignored and excluded from templates. `ui-assets generate` explicitly generates UI files only and does not publish the complete editor Export record. Handwritten foundations and genuine shared UI helpers belong in `Source.UIBase`; window business Controllers are private classes in their owning window module; independent row and Scene Controllers retain their own modules. Do not organise by control shape.
- The sole asset identity is its extensionless path relative to `Data/UI/Assets`, with `/` separators. Nested references use `Project:<relative path>`. Do not add `assetId`, a UI Catalog or old-path aliases.
- When moving an asset, preserve asset-local unique node names, `palette.exposed` and `palette.category`. Update nested references in managed JSON and handwritten `require` paths or references to renamed nodes, then run `ScriptTools ui-assets generate <project>`. `Ui.DefineWindow(ViewClass, definition, nativeBase?)` and `Ui.Define(ViewClass, definition, base?)` derive the key from the generated View; its `controls` and `assets` fields supply typed references without handwritten lookup lists or type casts. Use `--check` to verify generation without writing; generation requires no Preview registry.
- Generated `View.new()` creates the complete UI tree without a Controller: `controls` holds native controls, `assets` holds recursively constructed child Views, and `instance` exposes the native asset instance. Mount or attach the View explicitly. Use `Ui.Define` for independent row or Scene Controllers, binding an existing matching View with `Controller.new(model, ui)`. Reuse `self.ui.assets.Name` for child behaviour instead of wrapping its native instance again. Pure display may use the generated View directly; Controller discovery is unnecessary.
- A window module contains a local private Controller and returns `Ui.DefineWindow(ViewClass, Controller, nativeBase?)`, defaulting to `Engine.Canvas`. The framework creates the host and complete View, binds them and attaches the root before optional business `init(...)`; `self.ui`, `self.view`, `self.root` and `self.host` are already usable. It then prepares the Controller, calls optional `ready` and applies the initial hidden state. An empty Controller displays the original template. Use `windowOptions` for initial host behaviour, `Window.new(...businessArgs):mount(uiManager)` to register a root, and `self:createChild(name, ChildWindow, ...businessArgs)` to bind an existing authored child View. Do not repeat native constructors, static rectangles, `addChild` or child disposal lists in business Controllers.
- Keep business state on the private Controller and use public host methods for native state. Write instance methods in `camelCase` and static helpers in `PascalCase`; the factory supplies public UI entry points. Maintain only the Controller type and methods in the matching window stub with bare `---@meta`; `ui-assets generate` derives public UI declarations under `stub/Source/UIWindows`. A name such as `Source.Windows.WindowItem.Controller` is a private type, not a requireable module or public field. UI, Controller and generated View stay separate objects.
- Use `self:watch(target, field, Controller.method, immediate?)` for native field observations tied to Controller lifetime. It defaults to an immediate `(self, currentValue, Class.MISSING)` call and then receives `(self, newValue, oldValue)` on changes; the returned stop function cancels that subscription. Reuse native equality and nil-assignment rules, and do not implement recursive reactive proxies. Use `bindCallback` for native UI events and explicit refresh events for larger model changes.
- Keep static hierarchy, row kinds and layout in JSON, and reuse its generated instances. For model-dependent rows, use `self:createCollection(container, RowControllerClass)`, then `add(model, logicalSize?)`, `items`, `clear()` and `layout()`; do not maintain duplicate row construction or disposal lists. Views own their static children, bound Controller and dynamic collections. Dispose the owning window, View or Controller at the end of its lifetime. View disposal automatically clears native callbacks on generated controls; custom Controller disposal only releases resources or callbacks outside the owned View tree before inherited disposal.
- Keep JSON declarative. Controllers own callbacks, dynamic text and model access; native controls own rendering and primitive interaction. Apply [ludork-lua](../ludork-lua/SKILL.md) to Lua edits and [ludork-bindings](../ludork-bindings/SKILL.md) to binding changes.

Run the applicable UI checks in [ludork-verify](../ludork-verify/SKILL.md). For visible changes, inspect the affected editor form or game UI, including input/focus or nested-asset behaviour where relevant.
