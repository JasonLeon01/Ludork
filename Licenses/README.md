# Licence Text Index

Ludork uses the distribution-root [Zlib License](../LICENSE.md); third-party components and assets keep their own terms. See [THIRD_PARTY_NOTICES.md](https://jasonleon01.github.io/Ludork/notices/?version=1.0&lang=en_GB) for versions, sources, purposes and text paths.

This is the canonical `Licenses` tree. The editor package receives it in full; templates receive this index, the common runtime directories, and `FFmpeg` only when enabled.

- `DotNet`, `DotNetPackages`, `Avalonia`, and `EditorPackages`: editor and managed-runtime notices.
- `ScriptTools`: CPython, Nuitka runtime, OpenSSL, and Pillow with its incorporated-component notices for the packaged build tool runtime directory.
- `Lua`, `LuaSF`, `SFML`, `LuaGlue`, `lua-cjson`, `zlib`, and `NativeDependencies`: common template runtime notices.
- `FFmpeg`: optional video-runtime notices for FFmpeg templates.
- `GNUMake` and `MicrosoftVisualCppRuntime`: editor/external redistribution terms, excluded from templates.
- `HarmonyOSSans` and `SampleMusic`: canonical asset notices; template copies stay beside the assets.

The Gradle wrapper used by Android packaging keeps its complete licence and bilingual source notice beside the tool in each C++ Source template, under `Engine/PlatformHosts/Android/gradle/wrapper`.

The editor's `Avalonia.Controls.WebView` 12.1.0 uses the [MIT licence text](EditorPackages/Avalonia.Controls.WebView-LICENSE.txt) from the [upstream commit identified by its NuGet package](https://github.com/AvaloniaUI/Avalonia.Controls.WebView/blob/b45e042d21d96371bb6d07822a55c85ee5f74d2f/LICENSE).

The editor's `Avalonia.Labs.Notifications` 12.0.2 uses the [MIT licence text](EditorPackages/Avalonia.Labs.Notifications-LICENSE.txt) from its [NuGet source commit](https://github.com/AvaloniaUI/Avalonia.Labs/blob/fe1fd16ba0f18540afcdc5007b57f4dbb8528712/LICENSE) for native task-completion notifications.

The editor SVG dependencies use [Svg.Skia's MIT text](EditorPackages/Svg.Skia-LICENSE.txt) for `Svg.Controls.Avalonia`, `Svg.Model`, `Svg.SceneGraph`, and `ShimSkiaSharp`; [Svg.Custom's Microsoft Public License](EditorPackages/Svg.Custom-LICENSE.txt) for `Svg.Custom`; and [ExCSS's MIT text](EditorPackages/ExCSS-LICENSE.txt) for `ExCSS`. These texts are copied unchanged from the repository commits identified by the restored NuGet packages and linked in the notices table.

Legal texts are retained in their supplied language and are not translated or rewritten. Explanatory indexes do not replace those texts.
