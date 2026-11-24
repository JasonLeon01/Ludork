# Default Template Overview

The Game project provides playable default gameplay and supplies the project templates. Use it as a starting point for your own game rules, interfaces and resources.

## Entry and modules

Application naming and release settings follow [Packaging](<../../Editor User Guide/Run Debug and Package.md#packaging>).

`Scripts/Entry.lua` initialises the game and enters the native loop. Its module loading order is described in [Lua Runtime and Modules](<../Lua Runtime and Modules.md#entry-and-module-order>).

Startup calls the following functions explicitly. Requiring their modules does not initialise services:

| Call | Responsibility |
|---|---|
| `Engine.setAppName(...)` | Establishes the application's user directory and resource overrides before loading project modules. |
| `Source.Locale.Core.Init()` | Loads locale dictionaries before loading the configuration and selecting the language. |
| `Source.Configs.Main.LoadOrCreate()` | Reads the [platform game configuration](<Runtime Data Configuration and Saves.md#configuration>), creating it with default settings when absent. |
| `Source.Data.InitializeRuntime()` | Supplies project callbacks and preview base classes to `Global.Data.InitializeRuntime(options)`, which creates shared data services and registers data and Blueprint providers before the first scene. |
| `Source.System.InstallRuntimeProviders()` | Registers the configuration provider before the first scene. |
| `Source.System.Init()` | Applies game, audio and [shared Gameplay configuration](<Runtime Data Configuration and Saves.md#shared-gameplay-configuration>), and prepares display resources through `GlobalCore.Display` before the native loop. |

Entry also initialises latent graphs and the native System before it constructs `SceneInit`. Register providers once per session. Hot reload does not rerun startup; see [Lua Runtime and Modules](<../Lua Runtime and Modules.md#lua-hot-reload>). [Runtime Data, Configuration and Saves](<Runtime Data Configuration and Saves.md>) describes the configuration and save locations.

On every launch, `SceneInit` fades in a full-screen image with “Made With Ludork Engine” in white at the centre of a grey background. During the fade, it starts checking which materials need exporting, shader programs need compiling and animations need compressing. The image fades in over 0.30 seconds and stays fully visible for at least 0.20 seconds. If the checks are still running, it stays visible until they finish. It then fades out over 0.25 seconds before the regular Init background, progress bar and stage label appear.

The [required task](<../../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) exports materials, compiles shaders and compresses animations in that order, skipping stages with no work, then loads shared game data. Each stage that runs occupies an equal share of the progress bar. The label below the bar follows the current game language. When all caches are current, the only progress label is “Loading game data…”. The title scene opens when loading completes. A required failure stops startup. See [Planning graphics caches](<../Global and Core Modules/Core Modules/Engine/Maps and Materials.md#planning-graphics-caches>).

Mobile background/resume behaviour follows the [SceneBase lifecycle](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#scenebase>).

## Gameplay layout

- `Scripts/Global` contains shared infrastructure, including `Global.Data`, map audio under `Global.Audio` and `Global.Utils.MapPath`. The [data services](<Runtime Data Configuration and Saves.md#data-services>) contract separates these services from project startup and gameplay queries.
- `Scripts/Source/MapActors` contains the reusable map Actor classes. `Scripts/Source/Utils` contains gameplay helpers, including the Enemy and GameInstance implementation modules and `Source.Utils.AnimationCache`; scenes, business windows and components retain their own directories.
- `Scripts/GlobalFunctions` contains independent Lua Blueprint function libraries. Their paths in graph data begin with `GlobalFunctions.`; the native root module remains separate.
- `Scripts/Internal` contains Live Debug and UI foundations, including the shared window base classes.
- `Scripts/Internal/UI` contains Views generated from `Data/UI` by Export. Generated UI and locale files are absent from templates and version control; export the project before its first run. See [Declarative UI Authoring Workflow](<../Declarative UI/Authoring Workflow.md>).
- `Scripts/Mixins` contains directly attachable Lua behaviour.
- `Data/Blueprints` contains game-specific Actor classes.
- `Data/Materials` contains authored material graphs. SceneInit prepares their runtime shaders and metadata in the user data directory; templates contain the source graphs, not the cache.
- `Data/General` provides items, enemies, equipment, classes, players, states and special effects.
- `Data/Maps` and `Data/Tilesets` provide the playable world.
- `Engine/` contains the native runtime, modules and dependencies.

[Build and Module Layout](<../../Native C++ Development/Build and Module Layout.md>) describes native source ownership and dependencies.

## Template variants

Choose a native-source or Standalone variant using [Project Templates](<../../Getting Started/Project Templates.md>). Changes to the Game project do not update existing projects; apply them to each project or regenerate its template.

An existing Source project can also be [converted to Standalone](<../../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>), preserving its authored gameplay, declarations and current exports.

Runtime Lua, `Scripts/stub` declarations and `_meta.lua` file-layer rules are in [Lua Runtime and Modules](<../Lua Runtime and Modules.md>).
