# Ludork 1.0.0 许可证与第三方声明

Ludork 软件本体适用 [`LICENSE.md`](LICENSE.md) 中的 Zlib 许可证。该许可证适用于 Ludork 软件，不会替代下列组件和资产各自的许可证。

本声明覆盖 Ludork 1.0.0 的 Windows x64、macOS Apple Silicon 自包含编辑器包、随附工程模板，以及这些模板原样带入生成游戏包的 Ludork runtime 与资产材料。工程自行添加的代码和资产需要另行提供声明。许可证完整正文见 [`Licenses` 索引](Licenses/README_zh_CN.md)；C++ Source 模板还会在第三方源码树中保留原始声明。

## 编辑器与托管运行时

| 组件 | Ludork 1.0.0 使用版本 | 用途与分发范围 | 许可证 | 完整正文 | 官方来源 |
| --- | --- | --- | --- | --- | --- |
| .NET Runtime | 9.0.15 | 编辑器自包含运行时 | MIT 及其随附第三方条款 | `Licenses/DotNet/LICENSE.txt`、`Licenses/DotNet/THIRD-PARTY-NOTICES.txt` | [dotnet/runtime](https://github.com/dotnet/runtime) |
| CPython 及其包含的组件 | 每个发行包记录 Python 3.12 patch 版本；已审计 macOS 构建为 3.12.0 | 独立 `ScriptTools` 可执行文件的嵌入式运行时 | Python Software Foundation License 及 CPython 复现的随附许可证，包括 OpenSSL 3 许可证 | `Licenses/ScriptTools/Python-3.12-LICENSES-AND-ACKNOWLEDGEMENTS.rst.txt` | [python/cpython](https://github.com/python/cpython) |
| OpenSSL | 每个发行包记录实际版本；已审计 macOS 构建为 3.0.11 | macOS `ScriptTools` onefile payload 中的 `libssl` 与 `libcrypto`；Windows 可能包含等效的 Python runtime 库 | OpenSSL 3.0 及更高版本适用 Apache License 2.0 | `Licenses/ScriptTools/Python-3.12-LICENSES-AND-ACKNOWLEDGEMENTS.rst.txt`（OpenSSL 章节） | [openssl/openssl](https://github.com/openssl/openssl) |
| Nuitka runtime library | 4.1.3 | 独立 `ScriptTools` 可执行文件中的生成 runtime 代码 | AGPL 3.0；生成的 target code 适用 Nuitka Runtime Library Exception | `Licenses/ScriptTools/Nuitka-4.1.3-AGPL-3.0.txt`、`Licenses/ScriptTools/Nuitka-4.1.3-RUNTIME-EXCEPTION.txt`、`Licenses/ScriptTools/Nuitka-4.1.3-NOTICE.txt` | [Nuitka/Nuitka](https://github.com/Nuitka/Nuitka) |
| Zstandard | Nuitka onefile bootstrap 中的 1.4.7 | 独立 `ScriptTools` 可执行文件使用的压缩组件 | BSD 3-Clause | `Licenses/ScriptTools/Zstandard-1.4.7-LICENSE.txt` | [facebook/zstd](https://github.com/facebook/zstd) |
| Avalonia 软件包族 | 12.1.0 | 编辑器 UI、桌面后端、渲染、主题、颜色选择器与 Inter 字体集成 | MIT | `Licenses/Avalonia/LICENSE.txt` | [AvaloniaUI/Avalonia](https://github.com/AvaloniaUI/Avalonia) |
| Avalonia.AvaloniaEdit | 12.0.0 | 文本与代码编辑 | MIT | `Licenses/EditorPackages/AvaloniaEdit-LICENSE.txt` | [AvaloniaUI/AvaloniaEdit](https://github.com/AvaloniaUI/AvaloniaEdit) |
| Material.Avalonia | 3.17.0 | Material 样式与控件 | MIT | `Licenses/EditorPackages/Material.Avalonia-LICENSE.txt` | [AvaloniaCommunity/Material.Avalonia](https://github.com/AvaloniaCommunity/Material.Avalonia) |
| CommunityToolkit.Mvvm | 8.4.2 | 编辑器 ViewModel 基础设施 | MIT 及其随附第三方声明 | `Licenses/EditorPackages/CommunityToolkit.Mvvm-LICENSE.md`、`Licenses/EditorPackages/CommunityToolkit.Mvvm-THIRD-PARTY-NOTICES.txt` | [CommunityToolkit/dotnet](https://github.com/CommunityToolkit/dotnet) |
| MoonSharp | 2.0.0 | 在独立环境中读取编辑器侧 Lua metadata | BSD 3-Clause | `Licenses/EditorPackages/MoonSharp-LICENSE.txt` | [moonsharp-devs/moonsharp](https://github.com/moonsharp-devs/moonsharp) |
| NAudio 包族（NAudio、Core、Asio、Midi、Wasapi 与 WinMM） | 2.2.1 | Windows 及共享编辑器音频服务 | MIT | `Licenses/EditorPackages/NAudio-LICENSE.txt` | [naudio/NAudio](https://github.com/naudio/NAudio) |
| NAudio.Vorbis | 1.5.0 | 编辑器 Vorbis 音频集成 | MIT | `Licenses/EditorPackages/NAudio.Vorbis-LICENSE.txt` | [naudio/Vorbis](https://github.com/naudio/Vorbis) |
| NVorbis | 0.10.4 | 托管 Vorbis 解码器 | MIT | `Licenses/EditorPackages/NVorbis-LICENSE.txt` | [NVorbis/NVorbis](https://github.com/NVorbis/NVorbis) |
| NodifyM.Avalonia | 12.0.0 | Blueprint 图控件 | MIT | `Licenses/EditorPackages/NodifyM.Avalonia-LICENSE.txt` | [MakesYT/NodifyM.Avalonia](https://github.com/MakesYT/NodifyM.Avalonia) |
| Microsoft.CodeAnalysis.CSharp 与 Common | 5.6.0 | 编译具有完全信任权限的 C# 源码插件 | MIT 及其随附第三方声明 | `Licenses/EditorPackages/Roslyn-LICENSE.txt`、`Licenses/EditorPackages/Roslyn-THIRD-PARTY-NOTICES.rtf` | [dotnet/roslyn](https://github.com/dotnet/roslyn) |
| SkiaSharp 与原生资产 | 3.119.4 | Avalonia 图形后端 | MIT 及原生资产随附第三方声明 | `Licenses/EditorPackages/SkiaSharp-LICENSE.txt`、`Licenses/EditorPackages/SkiaSharp-and-HarfBuzzSharp-NativeAssets-THIRD-PARTY-NOTICES.txt` | [mono/SkiaSharp](https://github.com/mono/SkiaSharp) |
| HarfBuzzSharp 与原生资产 | 8.3.1.3 | 文本塑形 | MIT 及原生资产随附第三方声明 | `Licenses/EditorPackages/HarfBuzzSharp-LICENSE.txt`、`Licenses/EditorPackages/SkiaSharp-and-HarfBuzzSharp-NativeAssets-THIRD-PARTY-NOTICES.txt` | [mono/SkiaSharp](https://github.com/mono/SkiaSharp) |
| System.Reactive | 6.0.1 | 响应式事件基础设施 | MIT | `Licenses/EditorPackages/System.Reactive-LICENSE.txt` | [dotnet/reactive](https://github.com/dotnet/reactive) |
| MicroCom.Runtime | 0.11.6 | Avalonia 原生互操作 | MIT | `Licenses/EditorPackages/MicroCom.Runtime-LICENSE.txt` | [AvaloniaUI/MicroCom](https://github.com/AvaloniaUI/MicroCom) |
| Microsoft.IO.RecyclableMemoryStream | 3.0.1 | 开发构建中的诊断传输依赖 | MIT | `Licenses/EditorPackages/Microsoft.IO.RecyclableMemoryStream-LICENSE.txt` | [microsoft/Microsoft.IO.RecyclableMemoryStream](https://github.com/microsoft/Microsoft.IO.RecyclableMemoryStream) |
| Microsoft.Extensions 依赖注入与日志抽象 | 8.0.0 | 托管基础设施依赖 | MIT 及其随附第三方声明 | `Licenses/DotNetPackages/Microsoft.Extensions-8.0.0-LICENSE.txt`、`Licenses/DotNetPackages/Microsoft.Extensions-8.0.0-THIRD-PARTY-NOTICES.txt` | [dotnet/runtime](https://github.com/dotnet/runtime) |
| System.Collections.Immutable 与 System.Reflection.Metadata | 10.0.1 | Roslyn 依赖 | MIT 及其随附第三方声明 | `Licenses/DotNetPackages/System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-LICENSE.txt`、`Licenses/DotNetPackages/System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-THIRD-PARTY-NOTICES.txt` | [dotnet/dotnet](https://github.com/dotnet/dotnet) |
| System.IO.Pipelines | 8.0.0 | 托管传输依赖 | MIT 及其随附第三方声明 | `Licenses/DotNetPackages/System.IO.Pipelines-8.0.0-LICENSE.txt`、`Licenses/DotNetPackages/System.IO.Pipelines-8.0.0-THIRD-PARTY-NOTICES.txt` | [dotnet/runtime](https://github.com/dotnet/runtime) |
| System.Memory | 4.5.3 | NVorbis 依赖 | MIT 及其随附第三方声明 | `Licenses/DotNetPackages/System.Memory-4.5.3-LICENSE.txt`、`Licenses/DotNetPackages/System.Memory-4.5.3-THIRD-PARTY-NOTICES.txt` | [dotnet/corefx](https://github.com/dotnet/corefx) |
| System.ValueTuple | 4.5.0 | NVorbis 依赖 | MIT 及其随附第三方声明 | `Licenses/DotNetPackages/System.ValueTuple-4.5.0-LICENSE.txt`、`Licenses/DotNetPackages/System.ValueTuple-4.5.0-THIRD-PARTY-NOTICES.txt` | [dotnet/corefx](https://github.com/dotnet/corefx) |
| Microsoft.Win32.Registry、System.Security.AccessControl、System.Security.Principal.Windows 与 Microsoft.NETCore.Platforms | 4.7.0；平台元数据 3.1.0 | NAudio 包族引入的 Windows 支持；平台包仅包含兼容性元数据 | MIT 及其随附第三方声明 | `Licenses/DotNetPackages/Microsoft.Win32.Registry-and-System.Security-4.7.0-LICENSE.txt`、`Licenses/DotNetPackages/Microsoft.Win32.Registry-and-System.Security-4.7.0-THIRD-PARTY-NOTICES.txt` | [dotnet/corefx](https://github.com/dotnet/corefx) |
| Tmds.DBus.Protocol | 0.94.1 | Avalonia FreeDesktop 还原依赖；会从 macOS 与 Windows 正式包中移除 | MIT | `Licenses/EditorPackages/Tmds.DBus.Protocol-LICENSE.txt` | [tmds/Tmds.DBus](https://github.com/tmds/Tmds.DBus) |
| Avalonia ANGLE Windows natives | 2.1.27548.20260419 | Windows 包中的 OpenGL ES 转换层 | BSD 3-Clause | `Licenses/Avalonia/ANGLE-LICENSE.txt` | [AvaloniaUI/angle](https://github.com/AvaloniaUI/angle) |
| Avalonia.Fonts.Inter 中的 Inter 字体 | 随 Avalonia.Fonts.Inter 12.1.0 分发 | 默认编辑器字体资源 | SIL Open Font License 1.1 | `Licenses/Avalonia/Inter-OFL-1.1.txt` | [rsms/inter](https://github.com/rsms/inter) |

每个发行包都会在 `tools/ScriptTools-runtime-versions.txt` 中记录 ScriptTools 实际使用的 CPython、OpenSSL、Nuitka 与压缩工具版本。`python-zstandard` 0.25.0 只在构建 onefile payload 时使用，不会以 Python 包形式随发行版分发。

`AvaloniaUI.DiagnosticsSupport` 2.2.3 以及 analyser/build-service 软件包仅供开发使用，不进入 Release 包。编辑器打包脚本会移除不属于目标平台的程序集。

## 原生运行时与工程模板

| 组件 | 版本或 revision | 用途与分发范围 | 许可证 | 完整正文 | 官方来源 |
| --- | --- | --- | --- | --- | --- |
| LuaSF | v3.1.0.5-ME-OH | 所有工程模板与 UI preview host 中的 SFML 绑定 | MIT | `Licenses/LuaSF/LICENSE.txt` | [JasonLeon01/LuaSF-AutoGenerator](https://github.com/JasonLeon01/LuaSF-AutoGenerator) |
| Lua | 5.5.0 | 游戏脚本运行时与 `luac` | MIT | `Licenses/Lua/LICENSE.txt` | [Lua.org](https://www.lua.org/) |
| SFML | 3.1.0，SFML-ME 分支 310ME-OH | 图形、窗口、音频、系统与网络运行时 | Zlib/libpng | `Licenses/SFML/LICENSE.txt` | [JasonLeon01/SFML-ME `310ME-OH`](https://github.com/JasonLeon01/SFML-ME/tree/310ME-OH) |
| sol2 | 发行版 LuaSF 源码中的 3.2.3 | C++/Lua 绑定头文件 | MIT | `Licenses/sol2/LICENSE.txt` | [ThePhD/sol2](https://github.com/ThePhD/sol2) |
| lua-cjson | 源码发行版本 2.1.0.19；上游运行时常量报告为 2.1.0.11 | 链接到游戏运行时的 JSON 模块 | MIT | `Licenses/lua-cjson/LICENSE.txt` | [openresty/lua-cjson](https://github.com/openresty/lua-cjson) |
| zlib | 1.3.1 | 游戏运行时压缩支持 | Zlib | `Licenses/zlib/LICENSE.txt` | [madler/zlib](https://github.com/madler/zlib) |
| FreeType | 2.14.3 | 静态链接的 SFML 字体依赖 | FreeType License 或 GPLv2；Ludork 选用 FreeType License | `Licenses/NativeDependencies/FreeType-LICENSE.txt`、`Licenses/NativeDependencies/FreeType-FTL.txt` | [freetype/freetype](https://gitlab.freedesktop.org/freetype/freetype) |
| HarfBuzz | 14.1.0 | 静态链接的 SFML 文本塑形依赖 | Old MIT | `Licenses/NativeDependencies/HarfBuzz-COPYING.txt` | [harfbuzz/harfbuzz](https://github.com/harfbuzz/harfbuzz) |
| SheenBidi | 3.0.0 | 静态链接的 SFML 双向文字依赖 | Apache License 2.0 | `Licenses/NativeDependencies/SheenBidi-LICENSE.txt` | [Tehreer/SheenBidi](https://github.com/Tehreer/SheenBidi) |
| Ogg | 1.3.6 | 静态链接的 SFML 音频容器依赖 | BSD 3-Clause | `Licenses/NativeDependencies/Ogg-COPYING.txt` | [xiph/ogg](https://github.com/xiph/ogg) |
| Vorbis | 1.3.7 | 静态链接的 SFML 音频编解码依赖 | BSD 3-Clause | `Licenses/NativeDependencies/Vorbis-COPYING.txt` | [xiph/vorbis](https://github.com/xiph/vorbis) |
| FLAC | 1.5.0 | 静态链接的 SFML 音频编解码依赖 | BSD 3-Clause | `Licenses/NativeDependencies/FLAC-COPYING.Xiph.txt` | [xiph/flac](https://github.com/xiph/flac) |
| Mbed TLS | 3.6.5 | 静态链接的 SFML 网络加密依赖 | Apache License 2.0 或 GPLv2；Ludork 选用 Apache 选项 | `Licenses/NativeDependencies/MbedTLS-LICENSE.txt` | [Mbed-TLS/mbedtls](https://github.com/Mbed-TLS/mbedtls) |
| libssh2 | commit `704299e997bf518375dc9222670c57b800ac59e6`（1.11.2 开发分支） | 静态链接的 SFML SFTP 依赖 | BSD 3-Clause | `Licenses/NativeDependencies/libssh2-COPYING.txt` | [libssh2/libssh2](https://github.com/libssh2/libssh2) |
| SFML 随附的源码依赖 | SFML-ME 310ME-OH 随附 revision | SFML 源码模板中的 OpenGL 加载、图片、音频、Unicode、Vulkan、DirectInput 兼容与 Windows polling 支持 | SFML 与源码头所列 MIT、MIT-0、BSD、Apache 2.0、CC0 1.0、LGPL 2.1 或更高版本，或公有领域备选条款 | `Licenses/NativeDependencies/SFML-THIRD-PARTY.md`、`Licenses/NativeDependencies/Glad-CC0-1.0.txt`、`Licenses/NativeDependencies/Wine-DInput-LGPLv2.1.txt`；对应源码头文件中也保留完整声明 | [SFML-ME 依赖列表](https://github.com/JasonLeon01/SFML-ME/blob/310ME-OH/readme.md#external-libraries-used-by-sfml) |

## 可选工具、编解码器与资产

| 组件或资产 | 版本 | 分发范围 | 条款 | 完整正文或声明 | 官方来源 |
| --- | --- | --- | --- | --- | --- |
| FFmpeg | 8.1.2 | 仅限启用 FFmpeg 的模板；Windows/macOS 使用精简动态库，iOS、HarmonyOS 与 Android C++ Source 构建使用静态库，并随附完整源码包、补丁和构建配置 | Ludork 当前配置适用 LGPL 2.1 或更高版本；完整源码包还保留 FFmpeg 的 GPL/LGPL 许可证族，分发静态链接的 iOS、HarmonyOS 或 Android 应用还必须提供许可证要求的重新链接材料 | `Licenses/FFmpeg/README.md`、`Licenses/FFmpeg/UPSTREAM-LICENSE.md`、`Licenses/FFmpeg/COPYING.LGPLv2.1.txt`、`Licenses/FFmpeg/COPYING.LGPLv3.txt`、`Licenses/FFmpeg/COPYING.GPLv2.txt`、`Licenses/FFmpeg/COPYING.GPLv3.txt` | [FFmpeg](https://ffmpeg.org/) |
| GNU Make | 4.4.1 | Windows 编辑器内用于构建随附 FFmpeg 源码的工具 | GPL 3.0 或更高版本 | `Licenses/GNUMake/COPYING.txt`；`tools/gnu-make/COPYING` 会在工具旁另存一份 | [GNU Make](https://www.gnu.org/software/make/) |
| Microsoft Visual C++ Runtime | 打包时从 Visual Studio 2022 redistributable 安装中选取的版本 | Windows 编辑器 UI preview host 中未经修改的 app-local runtime 文件 | Microsoft Visual C++ Runtime 2015–2022 Software 许可证及打包者适用的 Visual Studio 再分发条款 | `Licenses/MicrosoftVisualCppRuntime/Visual-C-Runtime-2015-2022-License.docx`、`Licenses/MicrosoftVisualCppRuntime/README.md` | [Microsoft Runtime 许可证](https://visualstudio.microsoft.com/license-terms/vs2022-cruntime/)、[Visual Studio 2022 再分发](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution) |
| HarmonyOS Sans SC | 字体版权为 Copyright 2021 Huawei Device Co., Ltd. | 编辑器中的 `Assets/HarmonyOS_Sans_SC_Regular.ttf`，以及 Sample 和工程模板中的 `HarmonyOS_SansSC_Medium.ttf` | HarmonyOS Sans Fonts License Agreement | `Licenses/HarmonyOSSans/LICENSE.txt`；Sample 字体旁保留另一份副本 | [HarmonyOS 设计资源](https://developer.huawei.com/consumer/en/design/resource/) |
| “To Walk the Unseen Path” | Sample 资产 | Sample 中当前存在的 MP3 音乐 | Suno Free/Basic 条款将使用限制为个人非商业用途；没有可核验的原始模板再分发授权 | `Licenses/SampleMusic/NOTICE.md`；曲目旁保留声明副本 | [Suno 服务条款](https://suno.com/terms/) |

Sample 音乐的限制只适用于该音乐资产，Ludork 软件仍使用 Zlib 许可证。当前 Suno 条款不能证明可在编辑器或工程模板中再分发原始曲目，即使发行本身不商用也一样。发布包含该曲目的任何包前，必须移除或替换曲目，或取得明确的再分发权。

## 来源与商标

项目名称与商标属于各自权利人。来源地址与上游归属记录在本声明及对应许可证文件中。Ludork 与上述上游项目不存在附属或背书关系。
