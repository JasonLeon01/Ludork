# Ludork Sample 1.0.0 许可证与第三方声明

Ludork 软件本体适用 [`LICENSE.md`](LICENSE.md) 中的 Zlib 许可证。该许可证适用于 Ludork 软件，不会替代下列组件和资产各自的许可证。

本声明覆盖 Ludork 1.0.0 的 Sample 工程、随附工程模板，以及这些模板原样带入生成游戏包的 Ludork 游戏运行时与随包资产材料。工程自行添加的代码和资产需要另行提供声明。游戏运行时许可证完整正文见[游戏运行时许可证索引](Licenses/README_zh_CN.md)，随包资产正文则保留在对应资产旁；C++ Source 模板还会在第三方源码树中保留原始声明。编辑器、托管运行时、预览宿主和构建工具依赖不属于本声明，其材料继续保留在 Ludork 编辑器发行包中。

## 原生游戏运行时

| 组件 | 版本或 revision | 用途与分发范围 | 许可证 | 完整正文 | 官方来源 |
| --- | --- | --- | --- | --- | --- |
| LuaSF | v3.1.0.5-ME-OH | Sample 与所有工程模板中的 SFML 绑定 | MIT | `Licenses/LuaSF/LICENSE.txt` | [JasonLeon01/LuaSF-AutoGenerator](https://github.com/JasonLeon01/LuaSF-AutoGenerator) |
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

## 视频运行时与随包资产

| 组件或资产 | 版本 | 分发范围 | 条款 | 完整正文或声明 | 官方来源 |
| --- | --- | --- | --- | --- | --- |
| FFmpeg | 8.1.2 | Sample 与启用 FFmpeg 的模板；Windows/macOS 使用精简动态库，iOS、HarmonyOS 与 Android C++ Source 构建使用静态库，并随附完整源码包、补丁和构建配置 | Ludork 当前配置适用 LGPL 2.1 或更高版本；完整源码包还保留 FFmpeg 的 GPL/LGPL 许可证族，分发静态链接的 iOS、HarmonyOS 或 Android 应用还必须提供许可证要求的重新链接材料 | `Licenses/FFmpeg/README.md`、`Licenses/FFmpeg/UPSTREAM-LICENSE.md`、`Licenses/FFmpeg/COPYING.LGPLv2.1.txt`、`Licenses/FFmpeg/COPYING.LGPLv3.txt`、`Licenses/FFmpeg/COPYING.GPLv2.txt`、`Licenses/FFmpeg/COPYING.GPLv3.txt` | [FFmpeg](https://ffmpeg.org/) |
| HarmonyOS Sans SC | 字体版权为 Copyright 2021 Huawei Device Co., Ltd. | Sample 和工程模板中的 `Assets/Fonts/HarmonyOS_SansSC_Medium.ttf` | HarmonyOS Sans Fonts License Agreement | `Assets/Fonts/LICENSE.txt` | [HarmonyOS 设计资源](https://developer.huawei.com/consumer/en/design/resource/) |
| “To Walk the Unseen Path” | Sample 资产 | Sample 中当前存在的 MP3 音乐 | Suno Free/Basic 条款将使用限制为个人非商业用途；没有可核验的原始模板再分发授权 | `Assets/Musics/LICENSE.md` | [Suno 服务条款](https://suno.com/terms/) |

Sample 音乐的限制只适用于该音乐资产，Ludork 软件仍使用 Zlib 许可证。当前 Suno 条款不能证明可在编辑器或工程模板中再分发原始曲目，即使发行本身不商用也一样。发布包含该曲目的任何包前，必须移除或替换曲目，或取得明确的再分发权。

## 来源与商标

项目名称与商标属于各自权利人。来源地址与上游归属记录在本声明及对应许可证文件中。Ludork 与上述上游项目不存在附属或背书关系。
