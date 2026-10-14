# Ludork

English | [简体中文](README_zh_CN.md)

Ludork is a 2D RPG editor with visual map and data editing, Blueprint graphs, Lua scripting and a C++20 runtime.

This editor supports **v1.0 Game Runtime**.

![The Ludork editor workspace](https://jasonleon01.github.io/Ludork/1.0/images/overview/editor-workspace.png)

## Platforms

| Target | Minimum system |
| --- | --- |
| Editor and desktop games | Windows 10 x64; macOS 13.3 on Apple Silicon |
| iOS games | iOS 16.3, arm64 |
| HarmonyOS games | HarmonyOS 6.0.2 / API 22, arm64-v8a |
| Android games | Android 7.0 / API 24, arm64-v8a |

Mobile packaging requires a C++ Source project and Apple Silicon macOS with the target platform's tools. Linux and Intel Mac editor packages are not provided. See [platform setup and packaging](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11).

## Get started

1. Download and install the package for your system from [GitHub Releases](https://github.com/JasonLeon01/Ludork/releases). Release packages include the .NET runtime.
2. Create a **Standalone** project for Lua, Blueprint and asset editing, or **C++ Source** for native development and mobile targets. Enable **FFmpeg** if you need video playback.
3. For C++ Source, run **Construct** first. Run **Export**, then **Play**.
4. Before distribution, replace the default `APP_NAME` in `Scripts/Entry.lua`, then use **File → Pack Project**.

Release editors download templates from the matching release and reuse valid cached archives offline. Development packages use local templates.

| Guide | Covers |
| --- | --- |
| [First project](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=01%2F01) | Create, export and run |
| [Project templates](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=01%2F02) | Standalone, C++ Source and FFmpeg |
| [Workspace and IDEs](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F01) | Editor workspace and opening projects in an IDE |
| [Run, debug and package](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11) | Desktop/mobile output and signing |
| [Native development](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F01) | Toolchains, modules and bindings |
| [Editor plug-ins](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=05%2F01) | Installation and management |

Editor plug-ins run with your full user permissions. Install only trusted source and publishers.

## Build from source

Requires .NET 9 SDK, Python 3.12 and CMake 3.21 or newer, plus Visual Studio 2022/MSVC x64 on Windows or Xcode Command Line Tools on Apple Silicon macOS.

Windows:

```bat
tools\init.bat
tools\run_editor.bat
```

macOS:

```sh
./tools/init.sh
./tools/run_editor.sh
```

Initialization prepares dependencies and the editor preview host. Build the Game project separately with `tools\build_cpp.bat Game Debug` or `./tools/build_cpp.sh Game Debug`.

See the [tool reference](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md) for build commands, CI packages and documentation maintenance.

## Documentation and support

- [Website](https://jasonleon01.github.io/Ludork/?lang=en_GB) · [Documentation](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB)
- [GitHub Issues](https://github.com/JasonLeon01/Ludork/issues): include the editor version, OS, template, reproduction steps and relevant Console output.

## Licence and assets

Ludork uses the [Zlib License](LICENSE.md), including commercial use. Dependencies and assets retain their own terms; see [Third-Party Notices](https://jasonleon01.github.io/Ludork/notices/?lang=en_GB&version=1.0) and the [licence-text index](Licenses/README.md). Preserve applicable notices and licence texts when distributing your game; packaging does not verify their legal completeness.

The Game project's sample music is restricted to personal, non-commercial use under its supplied Suno Free Tier terms, which do not establish raw-track redistribution rights. Remove or replace it, or obtain express redistribution rights, before publishing a package containing it. The bundled font retains its supplied terms.
