# 许可证正文索引

[English](README.md) | 简体中文

Ludork 使用 [Zlib 许可证](../LICENSE.md)；第三方组件和资产适用各自条款。[THIRD_PARTY_NOTICES_zh_CN.md](https://jasonleon01.github.io/Ludork/notices/?version=1.0&lang=zh_CN) 列出版本、来源、用途及许可证路径。

编辑器包完整携带本权威目录。随附的 `Server/LudorkServer.tar.gz` 在 `LudorkServer/third-party/README.md` 中提供服务端的独立第三方声明。模板携带本索引、公共运行时声明，以及启用时的 `FFmpeg` 声明。

| 目录 | 内容 |
|---|---|
| `DotNet`、`DotNetPackages`、`Avalonia`、`EditorPackages` | 编辑器和托管运行时声明 |
| `ScriptTools` | 随附的 CPython、Nuitka runtime、OpenSSL、Pillow 及其所含组件声明 |
| `Lua`、`LuaSF`、`SFML`、`LuaGlue`、`lua-cjson`、`zlib`、`IXWebSocket`、`NativeDependencies` | 模板公共运行时声明 |
| `FFmpeg` | 可选视频运行时声明 |
| `GNUMake`、`MicrosoftVisualCppRuntime` | 编辑器及外部再分发条款，不进入模板 |
| `HarmonyOSSans`、`SampleMusic` | 资产声明；模板副本留在资产旁 |

C++ Source 模板在 `Engine/PlatformHosts/Android/gradle/wrapper` 下随 Android Gradle wrapper 保留完整许可证与中英文来源说明。

编辑器依赖的许可证正文均从 NuGet 包标识的源码提交原样复制：

| 依赖包 | 许可证正文 | 来源 |
|---|---|---|
| `Avalonia.Controls.WebView` 12.1.0 | [MIT](EditorPackages/Avalonia.Controls.WebView-LICENSE.txt) | [上游提交](https://github.com/AvaloniaUI/Avalonia.Controls.WebView/blob/b45e042d21d96371bb6d07822a55c85ee5f74d2f/LICENSE) |
| `Avalonia.Labs.Notifications` 12.0.2 | [MIT](EditorPackages/Avalonia.Labs.Notifications-LICENSE.txt) | [上游提交](https://github.com/AvaloniaUI/Avalonia.Labs/blob/fe1fd16ba0f18540afcdc5007b57f4dbb8528712/LICENSE) |
| `Svg.Controls.Avalonia`、`Svg.Model`、`Svg.SceneGraph`、`ShimSkiaSharp` | [Svg.Skia MIT](EditorPackages/Svg.Skia-LICENSE.txt) | 第三方声明 |
| `Svg.Custom` | [Microsoft Public License](EditorPackages/Svg.Custom-LICENSE.txt) | 第三方声明 |
| `ExCSS` | [MIT](EditorPackages/ExCSS-LICENSE.txt) | 第三方声明 |

法律正文保留原始语言，不作改写。本索引不能替代正文。
