# Game Localisation Workflow

Author game strings in an Excel workbook (`Data/Locale/Locale.xlsx`) and use Official Locale Tools to export Lua language catalogues for editor hints and gameplay runtime formatting.

## Workflow

Ensure your project contains `Scripts/Source/Locale/Core.lua` and normal locale initialisation in `Scripts/Entry.lua`, and use a spreadsheet application that preserves cached formula values if your workbook uses formulas.

1. Install Official Locale Tools via [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).
2. Add string IDs and translations to `Data/Locale/Locale.xlsx`.
3. Select **Export** before running or packaging the project.
4. Format strings in scripts and data with `LOC("ID")` or `{ID}`.

## Workbook

Create `Data/Locale/Locale.xlsx` following these structure rules:

- **Headers:** Every worksheet uses row 1 as its header row. Column A must be `ID` (case-insensitive), and columns B onwards name target languages such as `en_GB` and `zh_CN`. Blank language headers are ignored, and at least one language column is required.
- **Rows and merging:** Rows with a blank ID are skipped. IDs are trimmed and case-sensitive. Multiple worksheets merge into one catalogue: duplicate IDs are reported with both the first and duplicate cell coordinates, and a non-empty later value overwrites the earlier value for that language.
- **Formulas and leading equals signs:** Formula cells export their cached value when present; without a cached value, the exported text is the formula minus its leading `=`. Enter `'=` or `'==` to export a literal `=` or `==`.

## Export

During unified editor **Export**, Official Locale Tools generates one UTF-8 Lua table per language at `Scripts/Source/Locale/<language>.lua`:

```lua
return {
    ["TITLE_START"] = "Start",
    ["TITLE_EXIT"] = "Exit",
}
```

- **Validation:** Language names must be non-empty, non-rooted and free of path separators or invalid filename characters. A missing workbook skips export without error; a malformed workbook stops Export.
- **Encoding:** Export preserves Unicode text (including C1 control characters) as UTF-8 and escapes quotes, backslashes and ASCII control characters with Lua escapes, ensuring editor hints and the runtime read identical strings.
- **Change detection and packaging:** The workbook and generated language files participate in [Export change detection](<../../Editor User Guide/Run Debug and Package.md#construct-export-and-play>). Game packaging exports and excludes `Locale.xlsx`, whereas [Standalone conversion](<../../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>) keeps both the workbook and generated catalogues. Command-line run or pack requires running Export first. Generated language files are git-ignored and omitted from project templates while handwritten `Core.lua` is preserved.

## Editor hints

Any editor field whose trimmed text is a single `{TITLE_START}` token can display a live hint. The provider reads `Scripts/Source/Locale/<EditorLanguage>.lua` and falls back to `en_GB.lua`. Spaces or nested braces invalidate the token.

## Runtime

`Scripts/Entry.lua` calls `Source.Locale.Core.Init()` to load language catalogues before reading the active language via `Source.Configs.Main.LoadOrCreate()` (requiring `Core` alone does not load catalogues). Bind a local `LOC` formatter in gameplay modules:

```lua
local Locale = require("Source.Locale.Core")

local LOC = Locale.ApplyStringLocaleFormat

local buttonText = LOC("TITLE_START")
local tabLabels = Locale.ApplyListLocaleFormat({ "MENU_LOAD", "MENU_SAVE" })
local message = LOC(
    "{ITEM_NEW}: {ITEM_NAME}"
)
```

- **String formatting (`ApplyStringLocaleFormat`):** Resolves either a bare key or every `{ID}` placeholder in a string. Missing keys stay unchanged, unavailable languages fall back to `en_GB` where available, and doubled `{{` / `}}` produce literal braces. General Data strings are not auto-translated — pass them through `LOC` when displaying them.
- **List formatting (`ApplyListLocaleFormat`):** `Locale.ApplyListLocaleFormat(values: string[]): string[]` formats each element in order and returns a new array without mutating the input. Use it for tab labels and option lists, and call it again on language change.
- **Language switching:** `Locale.SetLanguage` switches the active language and synchronously broadcasts `LocaleChanged`, and the Config window persists the selection. Subscribed UI refreshes text from its original locale keys without resetting selection, focus or an active dialogue.
- **Initial language and packaging:** A new game configuration selects the platform language if it exactly matches an available catalogue, otherwise `en_GB`, while preserving existing saved settings; see [Runtime configuration](<../../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#configuration>). Lua compilation and **Archive Assets, Data and Scripts (.ldpak)** process `Core.lua` and generated language tables along with the rest of `Scripts`.
- **Reloading:** Restart the game after exporting updated translations. [Lua hot reload](<../../Lua and Blueprint Scripting/Lua Runtime and Modules.md#lua-hot-reload>) preserves already-loaded language tables.

> **Note:** `Data/Locale/Locale.xlsx` is a project authoring source rather than a runtime database or save format. Plug-in UI localisation is independent of game language catalogues.
