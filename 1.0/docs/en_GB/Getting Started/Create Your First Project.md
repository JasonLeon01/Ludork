# Create Your First Project

Create a Standalone project, open its initial map and run it.

### Create the project

1. On the Start page, choose **New Project**.

![Ludork Start page with recent-project and project-creation actions](../../../images/getting-started/start-page.png)

*The Start page is the entry point for creating or reopening a project.*

2. Select **Standalone**.
3. Leave the FFmpeg option disabled unless the game must play video.
4. Choose an existing destination directory and enter a new project name.
5. Confirm the destination path and choose **Create**. Ludork opens `Main.proj` when creation finishes.

The first download needs an internet connection and an available template for your editor version. **Cancel** stops creation and removes unfinished files. See [Obtaining templates](<Project Templates.md#obtaining-templates>) for download and offline use.

![New Project window with the Standalone template selected](../../../images/getting-started/new-project.png)

*Standalone is the recommended first-project template because it includes a ready-to-run native runtime.*

### Save and run

1. Select a map in the map list.
2. Click **Export** to save, validate and generate the UI, locale and material runtime files. Resolve any errors before continuing; Play is disabled until the first successful export.
3. Click **Play** on the toolbar. The game runs in the embedded panel or an **Individual Window**, according to **Edit → Development Tools and Settings → Individual Window**. The setting is available on Windows only. On other hosts, the game always opens in an **Individual Window**.
4. Open **Console** to see runtime output. Project editing is disabled while the game runs unless [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) was enabled before Play.
5. Click the same button, now **Stop**, to stop the game and resume editing.

A new project has no `Main.ini`; the runtime creates it on the first run. See [Project Settings and Asset Layout](<../Editor User Guide/Project Settings and Asset Layout.md>) for the project’s files and folders.

## Related pages

- [Project Templates](<Project Templates.md>)
- [Lua Basics](<Lua Basics.md>)
- [Start Page, Workspace and Shortcuts](<../Editor User Guide/Start Page Workspace and Shortcuts.md>)
- [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>)
