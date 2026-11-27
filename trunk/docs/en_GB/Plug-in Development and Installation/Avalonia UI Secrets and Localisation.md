# Avalonia UI, Secrets and Localisation

Open a plug-in-owned Avalonia window, store credentials in the host secret store and present localised UI strings in the editor's active language without depending on internal editor types. Start from the [With a Window template](<Manifest and Minimal Plug-in.md#generated-template>) referencing `Ludork.Plugin.Avalonia` and the host-shared Avalonia assemblies, and register a menu or map context-menu command to open your window.

## Set the window owner

Cast `context.UserInterface` to `IAvaloniaPluginUserInterface` to pass the editor's owner window to `ShowDialog`:

```csharp
if (context.UserInterface is not IAvaloniaPluginUserInterface avaloniaUi)
{
    return PluginResult.Failed("Avalonia UI is unavailable.");
}

MyPluginWindow window = new();
await window.ShowDialog(avaloniaUi.Owner);
return PluginResult.Completed();
```

Set `WindowStartupLocation` to `CenterOwner` where appropriate. The plug-in owns its window, controls, view state, cancellation and disposal of bitmaps or other native resources. Never search `Application.Current` for an editor window or inject controls into editor-owned views.

### Host theme and shared UI utilities

- **Theme resources (`Ludork.Plugin.Avalonia.PluginTheme`):** Read semantic colours with `Brush("Surface")` or `Color("Accent")`, and access the UI font (with Chinese fallback) through `FontFamily`. Available keys: `Background`, `Surface`, `Input`, `Hover`, `Border`, `Text`, `TextMuted`, `TextDisabled`, `Accent`, `AccentMuted`, `AccentHover`, `AccentPressed`, `TextOnAccent`, `Error`, `Warning` and `Success`. Read these resources only after the editor application has initialised, and let standard controls inherit the host theme.
- **Hue normalisation (`Ludork.Plugin.Avalonia.EditorBitmapEffects`):** `NormalizeHue(double)` wraps degrees into `[0, 360)` and treats non-finite values or offsets within `0.0001` degrees of a full turn as zero (`IsNeutralHue(double)` applies the same rule).
- **Bitmap and BGRA hue shifting:** `CreateHueShiftedBitmap(Bitmap, double, CancellationToken)` returns a new bitmap matching source size and DPI even for a neutral offset (caller owns disposal). `ApplyHueShiftBgra(Span<byte>, int width, int height, int stride, double hue, CancellationToken)` modifies an unpremultiplied BGRA buffer in place, preserving alpha, fully transparent pixels and row padding. Both accept an optional cancellation token and use Avalonia's double-precision HSV conversion with nearest-integer RGB rounding (matching the colour picker, which preserves its last hue for greyscale colours).
- **Zoom anchoring (`EditorZoomInput` / `EditorZoomAnchor`):** `EditorZoomInput.PrimaryModifier` and `HasPrimaryModifier` map to Command on macOS and Control on other platforms. `EditorZoomAnchor.Capture(contentPoint, viewportPoint, origin, scale)` records the pointer anchor in content coordinates, and `Apply(scrollViewer, origin, scale)` consumes it after layout while clamping the offset to the viewport (`scale` must be positive and finite). Callers own coordinate conversion, layout subscriptions and viewport resets; `IsPending` and `Clear()` expose pending state.
- **Markdown syntax parsing (`MarkdownSyntax`):** `GetLines`, `GetInlineMatches` and `ReadTable` share parsing without loading resources or opening links. Pass `Profile.Documentation` for documentation images, strikethrough and table escaping, or `Profile.Assistant` for assistant messages and preserved non-pipe escapes. `ReadTable` returns `null` if no table begins on that line; otherwise `MarkdownTable` returns `Header`, `Alignments`, padded/truncated `Rows` and `ConsumedLines`. Rendering controls, styling, resource validation and link handling remain the plug-in's responsibility.

## Separate program and data paths

Treat `PluginDirectory` (imported program content) as read-only and write all settings, caches and history under `PluginDataDirectory`. Both paths reflect the plug-in's active installation scope (global shared data vs project-isolated data); never derive either path from the process working directory.

- **Atomic file helpers (`Ludork.Plugin.Abstractions.FilePersistence`):** `WriteAllTextAtomic` and `WriteAllTextAtomicAsync` write UTF-8 without BOM; `WriteAtomic` and `WriteAtomicAsync` handle stream-based formats. Each writes a same-directory temporary file, flushes to storage and replaces the target, cleaning up on failure. Async overloads check cancellation before replacement, and the synchronous `beforeCommit` callback runs after closing the temporary file. Validation, multi-file transactions and rollbacks remain the caller's responsibility.
- **Custom transaction primitives:** `CreateTemporaryPath`, `WriteDurable` (creates a new file and flushes to storage), `MoveFile`, `DeleteFile` and `MoveDirectory` support custom atomic workflows. Move and delete helpers retry transient Windows sharing or access failures for up to 10 seconds, clear read-only destination attributes before overwrite or deletion, and propagate errors to the caller.

## Store secrets

Use the plug-in-scoped `SecretStore` from the command context:

```csharp
await context.SecretStore.WriteAsync(
    "api-key",
    apiKey,
    context.CancellationToken);
string? stored = await context.SecretStore.ReadAsync(
    "api-key",
    context.CancellationToken);
```

- **Platform backends:** `ContainsAsync`, `ReadAsync`, `WriteAsync` and `DeleteAsync` store credentials in Windows Credential Manager or macOS Keychain with no plaintext-file fallback. On Linux, reads use environment variables while writes and deletes throw an exception naming the required variable (`LUDORK_PLUGIN_SECRET_<hash>` for global plug-ins; `LUDORK_PROJECT_PLUGIN_SECRET_<hash>` for project plug-ins).
- **Scoping:** Global credentials are scoped by plug-in ID; project credentials include the project scope so identical plug-in IDs and secret names across different projects never collide.
- **Handling rules:** Keep logical key names stable and scoped to one purpose. Never copy a retrieved secret into settings files, diagnostics, URLs or conversation logs.

## Localise plug-in UI

Read `IPluginRegistrar.EditorLanguage` during `Register` and load matching string resources shipped inside your plug-in directory, with an explicit fallback such as `en_GB`. Both `PluginMenuCommand.Label` and `PluginMapContextMenuCommand.Label` must be localised before registration, and window titles, buttons, validation messages and diagnostics must be localised inside the plug-in — the host never translates plug-in strings.

- **`Ludork.Plugin.Abstractions.PluginLocalizer`:** `LoadDirectory(pluginDirectory, language)` reads `locales/<language>.json` and falls back to the entire `en_GB.json` file only when the requested file is missing. `LoadCatalog(localePath, language)` reads one language-keyed JSON object and falls back to `en_GB` per key. `Text(key)` returns the key itself when missing, and `Format(key, arguments)` uses the current culture.
- **Separation from game catalogues:** Game localisation is a separate project data workflow; never reuse a plug-in's UI locale files as the game's runtime text catalogue.
- **Generated Lua strings (`Ludork.Plugin.Abstractions.LuaStringLiteral`):** Use `Quote(string)` or `Append(StringBuilder, string)` to emit complete double-quoted Lua literals that escape quotes, backslashes and ASCII control characters while preserving Unicode text (including C1 control characters) as UTF-8. Because decimal Lua escapes encode bytes rather than Unicode codepoints, never convert non-ASCII codepoints directly into decimal escapes.

> **Note:** `IAvaloniaPluginUserInterface` exposes only the owner window — never editor controls, services or styles. The plug-in importer rejects loose XAML files, and the host does not support live language switching for already-loaded plug-ins.
