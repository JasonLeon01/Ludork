# Create Your First Project

## Goal

Create a Standalone project, open its initial map and run it.

## Steps

### Create the project

1. On the Start page, choose **New Project**.

![Ludork Start page with recent-project and project-creation actions](../../../images/getting-started/start-page.png)

*The Start page is the entry point for creating or reopening a project.*

2. Select **Standalone**.
3. Leave the FFmpeg option disabled unless the game must play video.
4. Choose an existing destination directory and enter a new project name.
5. Review the final path, then choose **Create**. A release editor downloads the matching template or reuses its cached archive; a development editor copies its local template. Ludork opens `Main.proj` when creation finishes.

The first download requires an internet connection and a published template for this editor version. Progress appears at the bottom of the window; **Cancel** stops creation and removes unfinished files. See [Obtaining templates](<Project Templates.md#obtaining-templates>) for cache locations and unavailable-release handling. New projects always use the current editor's major.minor project version.

![New Project window with the Standalone template selected](../../../images/getting-started/new-project.png)

*Standalone is the recommended first-project template because it includes a ready-to-run native runtime.*

### Save and run

1. Select a map in the map list.
2. Click **Export** to save, validate and generate the UI and locale files. Resolve any errors before continuing; Play is disabled until the first successful export.
3. Click **Play** on the toolbar. The game runs in the embedded panel or an **Individual Window**, according to **Edit → Development Tools and Settings → Individual Window**. The setting is available on Windows only. On other hosts, the game always opens in an **Individual Window**.
4. Watch the Console for preparation and runtime output. Play in an **Individual Window** keeps the map visible and disables project editing by default. Enable [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) before Play to edit the running map from the editor.
5. Click the same button, now **Stop**, to stop the game and resume editing.

## Important project files

The table lists the files present in a working project.

| Path | Purpose |
|---|---|
| `Main.proj` | Project compatibility version, C++ source and FFmpeg capabilities, packaging settings and run-window preference. |
| `Main.ini` | Runtime settings, including the entry script, language, audio and display values. |
| `Scripts/Entry.lua` | Lua entry point. |
| `Data/` | Maps, Blueprints, General Data, animation, curves, Text Config and UI assets. |
| `Assets/` | Textures, audio, fonts, shaders and other file resources. |

A new project has no `Main.ini`. The runtime creates it on the first run.

## Related pages

- [Project Templates](<Project Templates.md>)
- [Lua Basics](<Lua Basics.md>)
- [Start Page, Workspace and Shortcuts](<../Editor User Guide/Start Page Workspace and Shortcuts.md>)
- [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>)
