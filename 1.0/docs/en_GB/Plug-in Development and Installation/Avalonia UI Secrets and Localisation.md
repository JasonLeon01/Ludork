# Avalonia UI, Secrets and Localisation

## Goal

Open a plug-in-owned Avalonia window, store a secret through the host credential service, and present labels in the current editor language without depending on editor implementation types.

## Prerequisites

- A valid source plug-in that references `Ludork.Plugin.Avalonia` and the host-shared Avalonia assemblies.
- A registered menu or map-item context-menu command.
- Localised string resources shipped inside the plug-in directory.

## Steps

### Obtain the real owner

```csharp
if (context.UserInterface is not IAvaloniaPluginUserInterface avaloniaUi)
{
    return PluginResult.Failed("Avalonia UI is unavailable.");
}

MyPluginWindow window = new();
await window.ShowDialog(avaloniaUi.Owner);
return PluginResult.Completed();
```

Set `WindowStartupLocation` to `CenterOwner` where appropriate. The plug-in owns the window, the controls, the view state, cancellation and the disposal of images and other resources. Do not search `Application.Current` for an editor window. Do not inject controls into editor-owned views.

Use `Ludork.Plugin.Avalonia.PluginTheme` for the editor’s shared appearance: `Brush("Surface")` or `Color("Accent")` reads a semantic colour, and `FontFamily` supplies the UI font with Chinese fallback. Available colour keys are `Background`, `Surface`, `Input`, `Hover`, `Border`, `Text`, `TextMuted`, `TextDisabled`, `Accent`, `AccentMuted`, `AccentHover`, `AccentPressed`, `TextOnAccent`, `Error`, `Warning` and `Success`. Read these resources after the editor application has initialised, and let standard controls inherit the host theme.

`Ludork.Plugin.Avalonia.EditorBitmapEffects` supplies the same hue processing used by editor previews. `NormalizeHue(double)` wraps degrees into `[0, 360)` and treats non-finite values or offsets within `0.0001` degrees of a full turn as zero; `IsNeutralHue(double)` uses that rule. `CreateHueShiftedBitmap(Bitmap, double, CancellationToken)` returns a new bitmap with the source size and DPI, including for a neutral offset; the caller owns its disposal. `ApplyHueShiftBgra(Span<byte>, int width, int height, int stride, double hue, CancellationToken)` modifies an unpremultiplied BGRA buffer in place, preserving alpha, fully transparent pixels and row padding. Both processing methods accept an optional cancellation token. They use Avalonia's double-precision HSV conversion and nearest-integer RGB rounding. The colour picker uses the same conversion while retaining its last hue for greyscale colours.

`EditorZoomInput.PrimaryModifier` and `HasPrimaryModifier` select Command on macOS and Control elsewhere. `EditorZoomAnchor.Capture(contentPoint, viewportPoint, origin, scale)` records the pointer anchor in content coordinates; after layout, `Apply(scrollViewer, origin, scale)` consumes it and clamps the new offset to the viewport. The scale must be positive and finite. The caller owns coordinate conversion, layout subscriptions and viewport resets; `IsPending` and `Clear()` expose the pending state.

`MarkdownSyntax.GetLines`, `GetInlineMatches` and `ReadTable` share parsing without loading resources or opening links. Pass `Profile.Documentation` for document images/strikethrough syntax and documentation table escaping, or `Profile.Assistant` for message syntax and preserved non-pipe escapes. `ReadTable` returns null when no table starts at the given line; otherwise its `MarkdownTable` contains `Header`, `Alignments`, padded/truncated `Rows` and `ConsumedLines`. Controls, styling, resource validation and link actions remain the caller's responsibility.

### Separate program and data paths

`PluginDirectory` contains imported program content and must be treated as read-only. Write settings, caches and history beneath `PluginDataDirectory`. Do not derive either path from the current working directory.

`Ludork.Plugin.Abstractions.FilePersistence` provides `WriteAllTextAtomic` and `WriteAllTextAtomicAsync` for UTF-8 without BOM, and stream-based `WriteAtomic` / `WriteAtomicAsync` for other formats. They write a same-directory temporary file, flush it to storage and replace the destination, cleaning up on failure. Async writes check cancellation before replacement; the synchronous `beforeCommit` callback runs after closing the temporary file. Callers retain validation and multi-file transaction/rollback ownership.

`CreateTemporaryPath`, `WriteDurable`, `MoveFile`, `DeleteFile` and `MoveDirectory` support custom transactions. `WriteDurable` creates a new file and flushes to storage. Move/delete helpers retry transient Windows sharing/access failures for up to ten seconds; overwrite and deletion clear a read-only destination attribute. Errors propagate to the caller.

### Store secrets

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

`ContainsAsync`, `ReadAsync`, `WriteAsync` and `DeleteAsync` use the operating-system credential service supplied by the host. Never copy a returned secret into ordinary settings, diagnostics, URLs or conversation logs. Keep the logical key stable and scope it to one purpose.

### Localise plug-in UI

Read `IPluginRegistrar.EditorLanguage` during registration and load the matching plug-in resource, with an explicit fallback such as `en_GB`. `PluginMenuCommand.Label` and `PluginMapContextMenuCommand.Label` must already be localised when they are registered. Localise window titles, buttons, validation and diagnostics inside the plug-in. The host does not translate plug-in strings.

Use `Ludork.Plugin.Abstractions.PluginLocalizer.LoadDirectory(pluginDirectory, language)` for `locales/<language>.json`; it falls back to the whole `en_GB.json` file only when the requested file is absent. `LoadCatalog(localePath, language)` reads a single language-keyed JSON object and falls back to `en_GB` per key. `Text(key)` returns the key when unresolved, and `Format(key, arguments)` uses the current culture. Resource formats and language selection remain explicit.

Game localisation is a separate data workflow. Do not use the plug-in's own locale resources as the project's runtime text catalogue.

For generated Lua strings, use `Ludork.Plugin.Abstractions.LuaStringLiteral.Quote(string)` or `Append(StringBuilder, string)`. Both produce a complete double-quoted literal, escaping quotes, backslashes and ASCII control characters while preserving Unicode text, including C1 controls, as UTF-8 when the source file is written. Decimal Lua escapes represent bytes, so Unicode codepoints above ASCII must not be converted directly into decimal escapes. The shared representation is readable by both the native Lua runtime and the editor's readers.

## Limitations

`IAvaloniaPluginUserInterface` exposes only the owner window. It does not expose editor controls, services or styles. The source plug-in importer does not accept loose XAML source. The host does not provide live language switching to an already-loaded plug-in.

## Related pages

- [Manifest and Minimal Plug-in](<Manifest and Minimal Plug-in.md>)
- [Registration and Hook Reference](<Registration and Hook Reference.md>)
- [Game Localisation Workflow](<Official Plug-ins/Game Localisation Workflow.md>)
