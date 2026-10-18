# Game Localisation Workflow

Use Official Locale Tools to export a translation workbook into Lua language tables for gameplay.

## Prerequisites

- A project with `Scripts/Source/Locale/Core.lua` and the normal locale initialisation in `Scripts/Entry.lua`.
- A spreadsheet application capable of preserving cached formula values when formulas are used.

## Steps

1. Install Official Locale Tools through [the official plug-in installation](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).
2. Add an ID and translations to `Data/Locale/Locale.xlsx`.
3. Click **Export** before running the project.
4. Use `LOC("ID")` or `{ID}` as described below.

## Workbook

Create `Data/Locale/Locale.xlsx`. Every included worksheet must use row 1 as a header. Column A must be `ID` (case-insensitive), and columns B onwards are language names such as `en_GB` and `zh_CN`. Blank language headers are ignored, and at least one language is required.

Rows with a blank ID are ignored. IDs are trimmed and are case-sensitive. Multiple worksheets are merged. Duplicate IDs are reported with their first and duplicate cell locations, and a value present in the later cell overwrites the earlier value for the same language.

Formula cells use their cached value when one is present. Without a cached value, the formula text without the leading equals sign is exported. A literal `=` or `==` may be entered as `'=` or `'==`.

## Export

During the unified editor Export, the plug-in writes one UTF-8 Lua table per language:

```lua
return {
    ["TITLE_START"] = "Start",
    ["TITLE_EXIT"] = "Exit",
}
```

Export writes `Scripts/Source/Locale/<language>.lua`. Language names may not be blank or rooted, and may not contain separators or invalid filename characters. A missing workbook is skipped without error; a malformed workbook stops the operation.

Export preserves Unicode text, including C1 control characters, in UTF-8. Quotes, backslashes and ASCII control characters use Lua escapes. The editor hints and the game therefore read the same string values from the generated catalogue.

The workbook and generated catalogues participate in [Export change detection](<../../Editor User Guide/Run Debug and Package.md#construct-export-and-play>). Game packaging exports and excludes the workbook; [Standalone conversion](<../../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>) retains it and the generated catalogues. Direct command-line run or pack requires prior Export. Generated language files are ignored and excluded from templates; handwritten `Core.lua` is retained.

## Editor hints

A field whose complete trimmed text is `{TITLE_START}` can receive a hint. The provider reads the file matching `EditorLanguage` from `Scripts/Source/Locale` and falls back to `en_GB.lua`. Spaces and nested braces invalidate the token.

## Runtime

`Scripts/Entry.lua` calls `Source.Locale.Core.Init()` to load the language tables before reading the language setting with `Source.Configs.Main.LoadOrCreate()`. Requiring Core alone does not load them. Consumers bind a local `LOC` formatter:

```lua
local Locale = require("Source.Locale.Core")

local LOC = Locale.ApplyStringLocaleFormat

local buttonText = LOC("TITLE_START")
local tabLabels = Locale.ApplyListLocaleFormat({ "MENU_LOAD", "MENU_SAVE" })
local message = LOC(
    "{ITEM_NEW}: {ITEM_NAME}"
)
```

`ApplyStringLocaleFormat` resolves a bare key or each `{ID}` within a string. Missing keys remain unchanged, and an unavailable language uses `en_GB` where possible. Doubled `{{` and `}}` produce literal braces. General Data strings are not translated automatically, so call `LOC` when displaying them.

`Locale.ApplyListLocaleFormat(values: string[]): string[]` applies `ApplyStringLocaleFormat` to each array element in order and returns a new array without modifying the input. Use it for tab labels and option lists, and call it again when refreshing for a language change.

`Locale.SetLanguage` changes the active language and synchronously publishes `LocaleChanged`. The Config window saves the selection. Subscribed UI refreshes its text from the original locale keys without resetting the selection, the focus or an active dialogue.

A new `Main.ini` uses the platform language when it exactly matches a supported language, otherwise `en_GB`. Existing language settings are preserved. See [Runtime Data, Configuration and Saves](<../../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md>) for configuration defaults.

Lua compilation and **Archive Assets, Data and Scripts (.ldpak)** process Core and the generated language files with the rest of the Scripts tree. Official Locale Tools does not inspect either option or process the locale directory separately.

Restart the game after exporting changed translations. [Lua hot reload](<../../Lua and Blueprint Scripting/Lua Runtime and Modules.md#lua-hot-reload>) preserves loaded language tables.

## Limitations

The workbook is a project authoring source, not a runtime database or save format. Plug-in menu localisation is independent from these game catalogues.

## Related pages

- [Official Locale Tools](<Locale Tools.md>)
- [General Data and Text Config](<../../Editor User Guide/General Data and Text Config.md>)
