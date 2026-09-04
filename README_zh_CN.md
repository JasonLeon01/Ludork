# Ludork

[English](README.md) · [简体中文](README_zh_CN.md)

Ludork 是一款面向 2D 角色扮演游戏创作的游戏编辑器与原生运行时。它将可视化地图和数据创作、Blueprint 图、Lua 脚本与 C++20 运行时整合为一套完整的生产流程。

![Ludork 编辑器工作区](docs/_images/overview/editor-workspace.png)

## 核心能力

- 在统一编辑器内管理工程、地图、图块集、Actor、动画、曲线、General Data、Text Config 与声明式 UI 资产。
- 使用 Blueprint 图编写可视化玩法逻辑，并通过 Lua 模块和直接脚本挂载实现代码驱动的系统。
- 基于 LuaSF 与 SFML 的 C++20 运行时，以及自动生成的绑定、LuaLS 定义和 Blueprint metadata。
- 提供 Standalone、Standalone + FFmpeg、C++ Source 与 C++ Source + FFmpeg 四种工程模板。
- 通过拥有完全信任权限的 C# 13 编辑器插件添加命令、工具、工程操作与编辑器集成。

## 支持平台

| 范围 | 支持目标 | 说明 |
| --- | --- | --- |
| Ludork 编辑器 | Windows 10 或更高版本，x64 | 以自包含 Windows 安装包发布。 |
| Ludork 编辑器 | macOS 13.3 或更高版本，Apple Silicon | 以原生 `.app` 应用包发布。 |
| 桌面游戏工程 | Windows 10 或更高版本，x64；或 macOS 13.3 或更高版本，Apple Silicon | 新工程使用与编辑器宿主平台对应的模板。 |
| iOS 游戏工程 | iOS 15.0 或更高版本，arm64 | 打包需要 C++ Source 工程、Apple Silicon macOS、完整 Xcode 与 Apple 开发签名。 |
| HarmonyOS 游戏工程 | HarmonyOS 6.0.2 / API 22 或更高版本，arm64-v8a | Mobile 使用 OpenGL ES；2in1 默认使用 OpenGL，也可选择 OpenGL ES。HAP 打包需要 C++ Source 工程、Apple Silicon macOS 与 DevEco Studio。 |
| Android 游戏工程 | Android 7.0 / API 24 或更高版本，arm64-v8a | APK 打包需要 C++ Source 工程、Apple Silicon macOS、Android Studio、SDK Platform 36、Build Tools 36.0.0、Android NDK r27 或更高版本，以及本机 CMake 3.28 或更高版本。默认输出未签名 APK，也可使用已有 JKS 或 PKCS12 keystore 进行可选签名。 |

Ludork 1.0.0 不提供 Linux 或 Intel Mac 编辑器安装包。

## 安装正式版本

1. 打开 [Ludork Releases 页面](https://github.com/JasonLeon01/Ludork/releases)，下载与当前电脑对应的安装包。
2. 按照 Release Notes 安装或解压软件包。
3. 启动 Ludork 并选择界面语言。
4. 从启动页创建工程，或打开现有的 `Main.proj` 文件。

正式安装包为自包含发布，无需另外安装 .NET 运行时。macOS 可能会要求确认是否信任从互联网下载的应用。

## 选择工程模板

| 模板 | 适用场景 | 开发环境要求 |
| --- | --- | --- |
| **Standalone** | 立即开始编辑 Lua、Blueprint、地图、数据与资产。 | 不需要 C++ 工具链。 |
| **Standalone + FFmpeg** | 需要 Standalone 工作流及 H.264/AAC MP4 播放。 | 不需要 C++ 工具链；模板包含 FFmpeg 声明与源代码材料。 |
| **C++ Source** | 需要修改原生引擎代码、添加绑定、调试 C++，或输出 iOS/HarmonyOS/Android 目标。 | CMake 3.21 或更高版本，以及对应平台的编译工具链；Android 打包要求 CMake 3.28 或更高版本。 |
| **C++ Source + FFmpeg** | 同时需要原生源码与视频播放。 | 满足上述 C++ 要求；工程包含 FFmpeg 源码与构建配置。 |

FFmpeg 默认关闭。预编译的 FFmpeg Standalone 模板只面向桌面平台；iOS、HarmonyOS 或 Android 输出应使用 C++ Source 模板。这些平台包会静态链接各自的 FFmpeg 构建，因此还须履行 LGPL 对重新链接材料的额外要求；分发前应阅读随附的 FFmpeg 声明。

### 在 IDE 中调试 C++ Source 工程

C++ Source 工程根目录自带 IDE 配置生成工具。Windows 可运行 `generate_vs2022.bat` 后打开工程根目录下的 `Main.sln`，或运行 `generate_clion.bat` 后用 CLion 打开工程目录。Apple Silicon macOS 可运行 `./generate_clion.sh`，再用 CLion 打开工程目录。两套 CLion 生成工具都会在退出前完成 CMake configure，工程打开后可直接选择并运行 **Ludork Play**。

Visual Studio 的运行和调试会复用编辑器 Play 的同一个 Debug C++ 构建工具，以工程根目录作为工作目录，并执行相同的原生独立窗口流程。CLion 使用生成的 Debug profile 构建同一个 `Main` target。从 IDE 启动时不提供编辑器专属的保存处理、插件 Before Run hook、嵌入视口、命令桥或性能监控桥。

生成工具会自动查找 Ludork 安装目录中的工具。如果移动过编辑器或使用便携包，请先把 `LUDORK_TOOLS_DIR` 设为编辑器的 `tools` 目录。生成的 `Main.sln`、`.vs`、`.idea`、CMake 用户 preset 和 CLion 构建目录均为本机文件，已由模板 `.gitignore` 排除。

## 创建、运行并打包第一个工程

1. 选择**新建工程**；使用预编译桌面运行时时选择 **Standalone**，需要原生源码或 iOS/HarmonyOS/Android 打包时选择 **C++ Source**；填写工程名称和位置，然后选择**创建**。
2. 保存改动，并使用 **Play** 在编辑器内或独立游戏窗口中运行工程。
3. 正式分发前，将 `Scripts/Entry.lua` 中赋给 `APP_NAME` 的默认值 `LudorkSample` 改为游戏专属名称。
4. 选择**文件 → 打包项目**，选择目标和选项，并完成平台所需的提示。
5. 发布前，在干净的目标系统上测试生成的 `dist` 软件包。

从编辑器执行打包会运行已注册的插件准备 hook。Standalone 工程无需编译 C++ 即可编辑和打包；C++ Source 工程会执行所需的原生构建。

Android 打包不勾选**签名 APK**时生成 `dist/<游戏名>-android-arm64-v8a-unsigned.apk`；勾选后会打开已有 JKS 或 PKCS12 keystore 的签名窗口，并可在本机保存签名信息供以后使用。密码保存在操作系统凭据库中，不会写入项目。签名成功后只发布 `dist/<游戏名>-android-arm64-v8a-signed.apk`。同一已安装应用的后续版本必须复用相同签名密钥。两种模式都不会安装或启动 APK。

HarmonyOS 打包可选择使用 OpenGL ES 的 **Mobile** 手机/平板宿主，或选择 **2in1** 后再选择 OpenGL/OpenGL ES；2in1 默认使用 OpenGL。未签名产物分别写入 `dist/<游戏名>-harmony-mobile-unsigned.hap`、`dist/<游戏名>-harmony-2in1-opengl-unsigned.hap` 或 `dist/<游戏名>-harmony-2in1-opengl-es-unsigned.hap`。勾选**导出到匹配的 HarmonyOS 设备**后，会签名对应 HAP，将其安装到唯一一台形态匹配的已连接设备并启动应用。

## 从源码构建 Ludork

源码构建面向贡献者和编辑器开发者。普通用户通常应安装正式发行包。

环境要求：

- .NET 9 SDK
- CMake 3.21 或更高版本
- Windows：Windows 10 或更高版本、Visual Studio 2022 与 MSVC x64 工具链
- macOS：Apple Silicon、Xcode Command Line Tools 与 macOS 13.3 或更高版本
- 原生绑定生成工具需要 Python 3.12

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

初始化脚本会准备原生依赖和编辑器专用的 UI preview host，但不会构建 Sample 游戏。请另外使用 `tools\build_cpp.bat Sample Debug` 或 `./tools/build_cpp.sh Sample Debug` 构建 Sample。

## 文档与支持

- 从 [Ludork 文档](docs/zh_CN/00.Ludork%20文档.md)开始阅读。
- 按照[快速入门](docs/zh_CN/01.快速入门/01.创建第一个项目.md)完成工程创建、运行与打包。
- 通过 [GitHub Issues](https://github.com/JasonLeon01/Ludork/issues)报告可复现问题或提交功能建议。

报告问题时，请提供 Ludork 1.0.0、操作系统、所选模板、复现步骤及相关 Console 输出。请勿附带包含凭据或无权再分发内容的工程。

## 插件安全

Ludork 编辑器插件是会在编辑器进程中以当前用户完整权限执行的 C# 源码包。只导入和启用源代码及发布者均可信的插件；安装更新前应重新审查，不得将插件包视为沙箱内容。

## 许可证与资产权利

Ludork 软件本体使用 [Zlib 许可证](LICENSE.md)，允许商业使用。依赖项和随附工具继续适用各自条款；请查阅[第三方声明](THIRD_PARTY_NOTICES_zh_CN.md)及[许可证正文索引](Licenses/README_zh_CN.md)。

工程模板会把 Ludork 许可证，以及适用于游戏运行时、可选视频运行时和随包资产的声明与本地许可证完整正文带入新建工程。编辑器、托管运行时、预览宿主与构建工具声明只保留在编辑器发行包中，不会复制进工程。打包时会保留工程已有材料，但 Android APK 与 HarmonyOS HAP 打包不会把它们视为格式前置条件，也不校验法律材料是否完整。分发时应保留适用材料，为工程自行增加的依赖与资产补充声明，并重新审查最终包内容。

Sample 工程随附的音乐不适用 Ludork 的 Zlib 许可证。其 Suno Free Tier 条款将使用限制为个人非商业用途，不能据此证明可在工程模板中再分发原始曲目。发布任何包含该曲目的包之前，必须移除或替换曲目，或取得明确的再分发权。Sample 字体继续适用其随附条款。
