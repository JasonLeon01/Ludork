---
name: ludork-ui
description: Create or change Ludork Avalonia form inputs, declarative UI assets/controllers and native UI adapters. Use for input styling, UI asset moves, ownership and cross-layer UI contracts.
---

# Ludork UI

Choose the relevant section; an editor form change does not need the declarative runtime documentation.

## Editor form inputs

Use [Views/Utils/EditorInputs.cs](../../../Views/Utils/EditorInputs.cs) as the styling source of truth, matching Map Edit:

| Control | Factory or application API |
|---|---|
| Editable text | `CreateEditableTextBox()` / `ApplyEditable(textBox)` |
| Numeric input | `CreateNumericUpDown(...)` / `ApplyEditable(numeric)` |
| Browse-selected path or read-only display input | `CreateReadOnlyTextBox()` / `ApplyReadOnly(textBox)` |

Call these on `EditorInputs`; do not recreate their colours, borders, padding or focus behaviour. Read-only inputs must remain non-focusable, outside the tab order and free of an editable caret. Pure multiline display such as Markdown code blocks is exempt. Colour-picker numeric inputs may use `stretch: false` with a dedicated width.

## Declarative assets and controllers

Read [Authoring Workflow](<../../../docs/en_GB/03.Lua and Blueprint Scripting/05.Declarative UI/01.Authoring Workflow.md>) and the affected [Asset Schema](<../../../docs/en_GB/03.Lua and Blueprint Scripting/05.Declarative UI/04.Asset Schema and Control Registry.md>) or [Runtime Contract](<../../../docs/en_GB/03.Lua and Blueprint Scripting/05.Declarative UI/02.Runtime Contract.md>) section before editing. For native adapters, read [Native UI Adapters](<../../../docs/en_GB/04.Native C++ Development/08.Native UI Adapters.md>).

- Complete UIs live directly under `Data/UI/Assets`; internal parts belong in `Parts/<LargeUiName>` and shared parts in `Parts/Shared`. Mirror that ownership in `Scripts/Source/UI`, except genuine shared helpers. Do not organise by control shape.
- The sole asset identity is its extensionless path relative to `Data/UI/Assets`, with `/` separators. Nested references use `Project:<relative path>`. Do not add `assetId`, a UI Catalog or old-path aliases.
- When moving an asset, preserve asset-local unique node names, `palette.exposed` and `palette.category`. Update nested references in managed JSON and all matching `Ui.Define` keys, `require` paths and stub/LuaDoc paths in the same task.
- Keep JSON declarative. Controllers own callbacks, dynamic text and model access; native controls own rendering and primitive interaction. Apply [ludork-lua](../ludork-lua/SKILL.md) to Lua edits and [ludork-bindings](../ludork-bindings/SKILL.md) to binding changes.

Run the applicable UI checks in [ludork-verify](../ludork-verify/SKILL.md). For visible changes, inspect the affected editor form or game UI, including input/focus or nested-asset behaviour where relevant.
