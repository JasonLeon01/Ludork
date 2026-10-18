# Ludork Documentation

This guide covers Ludork 1.0. Downloads and project information are on the [Ludork website](https://jasonleon01.github.io/Ludork/?lang=en_GB); [third-party notices](https://jasonleon01.github.io/Ludork/notices/?version=1.0&lang=en_GB) are also linked from its footer.

Ludork combines a visual editor, Blueprint graphs, Lua scripts and an extensible C++20 runtime for map-driven 2D games. Use the editor to configure projects and create maps, Actors, data, animation and user interfaces, then run and package the game.

![The Ludork workspace with the Map List, layer tabs, map overview, tile palette and File Explorer](../../images/overview/editor-workspace.png)

## Choose a learning path

| If you want to… | Start here |
|---|---|
| Make a playable project | [Create Your First Project](<Getting Started/Create Your First Project.md>) |
| Author maps, assets and project data | [Start Page, Workspace and Shortcuts](<Editor User Guide/Start Page Workspace and Shortcuts.md>) |
| Configure Actor labels, lights, particles and child Actors | [Actor Components](<Editor User Guide/Actor Components/Overview.md>) |
| Look up what each Palette control does | [UI Controls in the Palette](<Editor User Guide/UI Controls in the Palette.md>) |
| Create material effects and instance parameter overrides | [Materials](<Editor User Guide/Materials.md>) |
| Build GPU particle effects for Actors and UI | [Common Particles](<Editor User Guide/Common Particles.md>) |
| Add timed subtitles to videos | [Video Subtitles](<Editor User Guide/Video Subtitles.md>) |
| Learn Lua | [Lua Basics](<Getting Started/Lua Basics.md>) |
| Write gameplay in Lua or Blueprint | [Lua Runtime and Modules](<Lua and Blueprint Scripting/Lua Runtime and Modules.md>) |
| Add or change native runtime APIs | [Build and Module Layout](<Native C++ Development/Build and Module Layout.md>) |
| Deploy optional remote account storage | [Simple Server](<Simple Server/Deployment and Editor Settings.md>) |
| Preview maps and assets or prepare the native preview host | [PreviewHost](<PreviewHost/Overview.md>) |
| Install or write a trusted editor extension | [Installing and Managing Plug-ins](<Plug-in Development and Installation/Installing and Managing Plug-ins.md>) |
| Find unreferenced project resources | [Official Resource Cleanup](<Plug-in Development and Installation/Official Plug-ins/Resource Cleanup.md>) |

## Supported platforms

### Editors

| Platform | Minimum supported version |
|---|---|
| Windows | Windows 10, x64 |
| macOS | Apple Silicon, macOS 13.3 |

### Games

| Platform | Minimum supported version | Notes |
|---|---|---|
| Windows | Windows 10, x64 | |
| macOS | Apple Silicon, macOS 13.3 | |
| iOS | iOS 16.3, arm64 | C++ Source project required. |
| HarmonyOS | HarmonyOS 6.0.2 / API 22, arm64-v8a | Mobile and 2in1; C++ Source project required. |
| Android | Android 7.0 / API 24, arm64-v8a | C++ Source project required. |

Mobile packages are produced on Apple Silicon macOS. A Standalone project uses the prebuilt runtime for the editor’s host platform. See [Run, Debug and Package](<Editor User Guide/Run Debug and Package.md>) for platform tools and signing options.

## Documentation conventions

- Paths are relative to the project root unless a page states otherwise.
- Menu commands and field labels are written as they appear in the English editor.

## Product and content licences

Ludork's software licence, third-party notices and Game project asset terms are separate. Review all three before redistributing an editor package or a game made from Game project content. Installing a plug-in also runs trusted code with the permissions of the editor process. Inspect the source and origin of a plug-in before import.
