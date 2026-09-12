# Licence Text Index

Ludork uses the distribution-root [Zlib License](../LICENSE.md); third-party components and assets keep their own terms. See [THIRD_PARTY_NOTICES.md](../docs/THIRD_PARTY_NOTICES.md) for versions, sources, purposes and text paths.

This is the canonical `Licenses` tree. The editor package receives it in full; templates receive this index, the common runtime directories, and `FFmpeg` only when enabled.

- `DotNet`, `DotNetPackages`, `Avalonia`, and `EditorPackages`: editor and managed-runtime notices.
- `ScriptTools`: CPython, Nuitka runtime, OpenSSL, and Pillow with its incorporated-component notices for the packaged build tool runtime directory.
- `Lua`, `LuaSF`, `SFML`, `sol2`, `lua-cjson`, `zlib`, and `NativeDependencies`: common template runtime notices.
- `FFmpeg`: optional video-runtime notices for FFmpeg templates.
- `GNUMake` and `MicrosoftVisualCppRuntime`: editor/external redistribution terms, excluded from templates.
- `HarmonyOSSans` and `SampleMusic`: canonical asset notices; template copies stay beside the assets.

The editor SVG dependencies use [Svg.Skia's MIT text](EditorPackages/Svg.Skia-LICENSE.txt) for `Svg.Controls.Avalonia`, `Svg.Model`, `Svg.SceneGraph`, and `ShimSkiaSharp`; [Svg.Custom's Microsoft Public License](EditorPackages/Svg.Custom-LICENSE.txt) for `Svg.Custom`; and [ExCSS's MIT text](EditorPackages/ExCSS-LICENSE.txt) for `ExCSS`. These texts are copied unchanged from the repository commits identified by the restored NuGet packages and linked in the notices table.

Legal texts are retained in their supplied language and are not translated or rewritten. Explanatory indexes do not replace those texts.
