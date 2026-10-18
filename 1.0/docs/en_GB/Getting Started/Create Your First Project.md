# Create Your First Project

Create a Standalone project, open its initial map and run it.

### Create the project

1. On the Start page, choose **New Project**.

![Ludork Start page with recent-project and project-creation actions](../../../images/getting-started/start-page.png)

2. Select **Standalone**.
3. Leave the FFmpeg option disabled unless the game must play video.
4. Choose an existing destination directory and enter a new project name.
5. Confirm the destination path and choose **Create**. Ludork opens `Main.proj` when creation finishes.

Release editors download the matching template on first use. See [Obtaining templates](<Project Templates.md#obtaining-templates>) for availability, offline use and cancellation.

![New Project window with the Standalone template selected](../../../images/getting-started/new-project.png)

### Save and run

1. Select a map in the map list.
2. Click **Export** and resolve any reported errors. Play requires a successful export.
3. Click **Play**. Windows supports embedded or individual-window mode; other hosts use an individual window. See [Construct, Export and Play](<../Editor User Guide/Run Debug and Package.md#construct-export-and-play>) for window settings and export details.
4. Open **Console** to see runtime output. Project editing is disabled while the game runs unless [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) was enabled before Play.
5. Click the same button, now **Stop**, to stop the game and resume editing.

A new project has no `Main.ini`; the runtime creates it on the first run. See [Project Settings and Asset Layout](<../Editor User Guide/Project Settings and Asset Layout.md>) for the project’s files and folders.

## Related pages

- [Lua Basics](<Lua Basics.md>)
- [Start Page, Workspace and Shortcuts](<../Editor User Guide/Start Page Workspace and Shortcuts.md>)
