# Ludork

[English](README.md) | 简体中文

Ludork 是一款 2D RPG 编辑器，集成可视化地图与数据编辑、蓝图、Lua 脚本和 C++20 运行时。

当前编辑器适用于 **v1.0 Game Runtime**。

## 支持平台

| 目标 | 最低系统要求 |
| --- | --- |
| 编辑器与桌面游戏 | Windows 10 x64；macOS 13.3，Apple Silicon |
| iOS 游戏 | iOS 16.3，arm64 |
| HarmonyOS 游戏 | HarmonyOS 6.0.2 / API 22，arm64-v8a |
| Android 游戏 | Android 7.0 / API 24，arm64-v8a |

移动端打包需要 C++ Source 项目、Apple Silicon macOS 及目标平台工具，不提供 Linux 或 Intel Mac 编辑器包。详见[平台准备与打包](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=02%2F11)。

## 快速开始

1. 从 [GitHub Releases](https://github.com/JasonLeon01/Ludork/releases) 下载并安装对应平台的发行包，无需另行安装 .NET 运行时。
2. 仅编辑 Lua、蓝图与资产时选择 **Standalone**；需要原生开发或移动端打包时选择 **C++ Source**。如需播放视频，启用 **FFmpeg**。
3. C++ Source 项目先执行 **编译**，再执行 **导出 → 运行**；Standalone 项目直接执行 **导出 → 运行**。
4. 发布前修改 `Scripts/Entry.lua` 中默认的 `APP_NAME`，然后选择 **文件 → 打包项目**。

正式版从对应 Release 下载模板，有效缓存可离线复用；开发版使用本地模板。

| 指南 | 内容 |
| --- | --- |
| [创建第一个项目](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=01%2F01) | 创建、导出与运行 |
| [项目模板](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=01%2F02) | Standalone、C++ Source 与 FFmpeg |
| [工作区与 IDE](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=02%2F01) | 编辑器工作区与使用 IDE 打开项目 |
| [运行、调试与打包](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=02%2F11) | 桌面与移动端输出、签名 |
| [原生开发](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=04%2F01) | 工具链、模块与绑定 |
| [编辑器插件](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN&doc=05%2F01) | 安装与管理 |

编辑器插件拥有当前用户的完整权限，请仅安装来源与代码均可信的插件。

## 从源码构建

需要 .NET 9 SDK、Python 3.12 和 CMake 3.21 或更高版本；Windows 另需 Visual Studio 2022/MSVC x64，Apple Silicon macOS 另需 Xcode Command Line Tools。

Windows：

```bat
tools\init.bat
tools\run_editor.bat
```

macOS：

```sh
./tools/init.sh
./tools/run_editor.sh
```

初始化会准备依赖和编辑器预览宿主。Game 项目需单独运行 `tools\build_cpp.bat Game Debug` 或 `./tools/build_cpp.sh Game Debug` 构建。

构建命令、CI 打包和文档维护见[工具说明](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md)。

## 文档与支持

- [官网](https://jasonleon01.github.io/Ludork/?lang=zh_CN) · [文档](https://jasonleon01.github.io/Ludork/docs/trunk/?lang=zh_CN)
- 通过 [GitHub Issues](https://github.com/JasonLeon01/Ludork/issues) 反馈问题，并提供编辑器版本、系统、模板、复现步骤和相关控制台输出。

## 许可证与资产

Ludork 使用 [Zlib 许可证](LICENSE.md)，允许商业使用。依赖和资产各自适用其条款，详见[第三方声明](https://jasonleon01.github.io/Ludork/notices/?lang=zh_CN&version=1.0)及[许可证正文索引](Licenses/README_zh_CN.md)。分发游戏时应保留适用声明和许可证正文；打包流程不校验法律材料的完整性。

Game 项目的示例音乐按随附 Suno Free Tier 条款仅限个人非商业使用，该条款未授予原始曲目的再分发权。发布包含该曲目的发行包前，请移除或替换曲目，或取得明确的再分发权。随附字体适用其原有条款。
