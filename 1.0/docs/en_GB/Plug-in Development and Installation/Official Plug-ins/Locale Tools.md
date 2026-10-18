# Official Locale Tools

`Plugins/OfficialLocaleTools` is the reference implementation for an ordinary menu command, a text hint provider, an Export hook and a separate before-Pack exclusion hook. It does not register a map-item context-menu command.

Install it with the [official plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>) and confirm **Loaded** in the plug-in manager.

## Registration

The public entry type loads the plug-in's own `locale.json`, then registers:

- A Database command at order 900 that opens `Data/Locale/Locale.xlsx`
- A `LocaleTextHintProvider`
- A `LocaleExportHook` through `RegisterBeforeExportHook`
- A before-Pack hook that only excludes the authoring workbook from final game packages; Standalone project conversion skips this hook and retains the workbook

The Export hook declares the workbook as input and language Lua files as outputs for change detection, and refreshes its cache and editor text hints after each export attempt.

The [Game Localisation Workflow](<Game Localisation Workflow.md>) covers workbook authoring, generated files and packaging.

## Menu behaviour

The menu handler requires an open project, checks that the workbook exists and asks the operating system to open it. It reports handled warnings and errors through `IPluginUserInterface`.

## Plug-in and game languages

`Plugins/OfficialLocaleTools/locale.json` localises the plug-in's own menu and diagnostic messages using `EditorLanguage`. It is not the game's catalogue.

## Related pages

- [Registration and Hook Reference](<../Registration and Hook Reference.md>)
