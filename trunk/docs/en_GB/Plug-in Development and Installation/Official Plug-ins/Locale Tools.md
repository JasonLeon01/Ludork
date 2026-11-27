# Official Locale Tools

`Plugins/OfficialLocaleTools` is the official plug-in for game translation workbooks and serves as the reference implementation for an ordinary menu command, a text-hint provider, an Export hook and a before-Pack exclusion hook (it registers no map-item context-menu command).

Install it through [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>) and confirm it reports **Loaded** in the **Plug-in Manager**.

## Registration

The entry type loads the plug-in's own `locale.json`, then registers four components:

- A **Database** menu command (order `900`) that opens `Data/Locale/Locale.xlsx`
- `LocaleTextHintProvider` for `{ID}` editor hints
- `LocaleExportHook` via `RegisterBeforeExportHook`
- A before-Pack hook that excludes the authoring workbook from final game packages (skipped during Standalone project conversion so the workbook stays intact)

`LocaleExportHook` declares `Data/Locale/Locale.xlsx` as its input and the generated Lua language files as outputs for Export change detection, refreshing its internal cache and editor text hints after every export attempt.

See [Game Localisation Workflow](<Game Localisation Workflow.md>) for workbook rules, generated Lua files and runtime usage.

## Menu behaviour

The menu handler requires an open project, verifies that `Data/Locale/Locale.xlsx` exists and asks the operating system to open it with the default spreadsheet application, reporting handled warnings and errors through `IPluginUserInterface`.

## Plug-in and game languages

`Plugins/OfficialLocaleTools/locale.json` localises the plug-in's own menu label and diagnostic messages using `EditorLanguage`. It is completely separate from the project's game language catalogues.
