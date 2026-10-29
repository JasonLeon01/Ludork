# Licence Text Index

English | [简体中文](README_zh_CN.md)

Ludork uses the [Zlib License](../LICENSE.md); third-party components and assets retain their own terms. [THIRD_PARTY_NOTICES.md](https://jasonleon01.github.io/Ludork/notices/?version=1.0&lang=en_GB) lists versions, sources, purposes and licence paths.

The editor package includes this canonical tree in full. The independently distributed Server archive carries server notices in `LudorkServer/third-party/README.md`. Templates include this index, common runtime notices and `FFmpeg` when enabled.

| Directories | Contents |
|---|---|
| `DotNet`, `DotNetPackages`, `Avalonia`, `EditorPackages` | Editor and managed-runtime notices |
| `ScriptTools` | Packaged CPython, Nuitka runtime, OpenSSL, Pillow and incorporated-component notices |
| `Lua`, `LuaSF`, `SFML`, `LuaGlue`, `lua-cjson`, `zlib`, `IXWebSocket`, `NativeDependencies` | Common template runtime notices |
| `FFmpeg` | Optional video-runtime notices |
| `GNUMake`, `MicrosoftVisualCppRuntime` | Editor/external redistribution terms; excluded from templates |
| `HarmonyOSSans`, `SampleMusic` | Asset notices; template copies stay beside the assets |

C++ Source templates retain the Android Gradle wrapper's complete licence and bilingual source notice beside it in `Engine/PlatformHosts/Android/gradle/wrapper`.

MinGW-built Windows games retain the compiler runtime licences and source-distribution information in [NativeDependencies/MinGW-RUNTIME-NOTICE.md](NativeDependencies/MinGW-RUNTIME-NOTICE.md), together with the WebView2 SDK loader licence and notices.

Editor package licence texts come unchanged from the source commits identified by their NuGet packages:

| Packages | Licence text | Source |
|---|---|---|
| `Avalonia.Controls.WebView` 12.1.0 | [MIT](EditorPackages/Avalonia.Controls.WebView-LICENSE.txt) | [Upstream commit](https://github.com/AvaloniaUI/Avalonia.Controls.WebView/blob/b45e042d21d96371bb6d07822a55c85ee5f74d2f/LICENSE) |
| `Avalonia.Labs.Notifications` 12.0.2 | [MIT](EditorPackages/Avalonia.Labs.Notifications-LICENSE.txt) | [Upstream commit](https://github.com/AvaloniaUI/Avalonia.Labs/blob/fe1fd16ba0f18540afcdc5007b57f4dbb8528712/LICENSE) |
| `Svg.Controls.Avalonia`, `Svg.Model`, `Svg.SceneGraph`, `ShimSkiaSharp` | [Svg.Skia MIT](EditorPackages/Svg.Skia-LICENSE.txt) | Third-party notices |
| `Svg.Custom` | [Microsoft Public License](EditorPackages/Svg.Custom-LICENSE.txt) | Third-party notices |
| `ExCSS` | [MIT](EditorPackages/ExCSS-LICENSE.txt) | Third-party notices |
| `SharpCompress` 0.50.7 | [MIT](EditorPackages/SharpCompress-LICENSE.txt) | [Upstream commit](https://github.com/adamhathcock/sharpcompress/blob/b4af8afe6683d019d504954a3b4e4f18b06e2cd6/LICENSE.txt) |

Legal texts retain their supplied language and are not rewritten. This index does not replace them.
