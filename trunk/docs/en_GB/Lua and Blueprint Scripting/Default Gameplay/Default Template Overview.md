# Default Template Overview

The Game project provides playable Default Gameplay and serves as the source for new project templates. Use it as a starting point for custom game rules, interfaces and assets.

## Entry and modules

Application naming and release settings follow [Packaging](<../../Editor User Guide/Run Debug and Package.md#packaging>). `Scripts/Entry.lua` initialises the runtime and hands control to the native loop; see [Entry and module order](<../Lua Runtime and Modules.md#entry-and-module-order>) for native module load order.

Startup calls the functions below explicitly — requiring their modules does not initialise services:

| Call | Responsibility |
|---|---|
| `Engine.setAppName(...)` | Sets the application's user directory and resource overrides before loading project modules. |
| `Source.Locale.Core.Init()` | Loads locale dictionaries before reading configuration and selecting the language. |
| `Source.Configs.Main.LoadOrCreate()` | Reads the [platform game configuration](<Runtime Data Configuration and Saves.md#configuration>), creating defaults if absent. |
| `Source.Data.InitializeRuntime()` | Passes project callbacks and preview base classes to `Global.Data.InitializeRuntime(options)`, creating shared data services and registering data and Blueprint providers before the first scene. |
| `Source.System.InstallRuntimeProviders()` | Registers the configuration provider before the first scene. |
| `Source.System.Init()` | Applies game, audio and [shared Gameplay configuration](<Runtime Data Configuration and Saves.md#shared-gameplay-configuration>), and prepares display resources via `GlobalCore.Display` before entering the native loop. |

`Entry` also initialises latent graphs and the native System before creating `SceneInit`. Register providers once per session; [Lua hot reload](<../Lua Runtime and Modules.md#lua-hot-reload>) does not rerun startup. See [Runtime Data, Configuration and Saves](<Runtime Data Configuration and Saves.md>) for configuration and save paths.

### `SceneInit` splash and cache preparation

On every launch, `SceneInit` fades in a full-screen splash image displaying `"Made With Ludork Engine"` centred in white on a grey background:

- **Timing**: Fades in over `0.30` seconds, stays fully visible for at least `0.20` seconds (or longer until initial cache checks complete), and fades out over `0.25` seconds before displaying the Init background, progress bar and stage label.
- **Preparation stages**: While fading in, `SceneInit` checks which materials need exporting, shaders need compiling and animations need compressing. Its [required task](<../../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) executes those three stages in order (skipping stages with no work), then loads shared game data.
- **Progress and completion**: Each active stage occupies an equal share of the progress bar, with a localised label below it. When all caches are up to date, the sole label is `"Loading game data..."`. Completing the task opens the title scene; any required failure halts startup. See [Planning graphics caches](<../Global and Core Modules/Core Modules/Engine/Maps and Materials.md#planning-graphics-caches>).

Mobile background and resume behaviour follows the [SceneBase lifecycle](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#scenebase>).

## Gameplay layout

| Path | Responsibility |
|---|---|
| `Scripts/Global` | Shared infrastructure (`Global.Data`, map audio under `Global.Audio`, `Global.Utils.MapPath`). Separated from project startup and gameplay queries by the [data services](<Runtime Data Configuration and Saves.md#data-services>) contract. |
| `Scripts/Source/MapActors` | Reusable map Actor classes. |
| `Scripts/Source/Utils` | Gameplay helpers, including Enemy and GameInstance implementation modules and `Source.Utils.AnimationCache`. Scenes, windows and components live in their own directories under `Scripts/Source`. |
| `Scripts/GlobalFunctions` | Independent Lua Blueprint function libraries (paths start with `GlobalFunctions.`, separate from the native root module). |
| `Scripts/Internal` | Live Debug and UI foundations, including shared window base classes. |
| `Scripts/Internal/UI` | Lua Views generated from `Data/UI` by **Export**. Generated UI and locale files are omitted from templates and version control; export the project before running it for the first time (see [Declarative UI Authoring Workflow](<../Declarative UI/Authoring Workflow.md>)). |
| `Scripts/Mixins` | Directly attachable Script Mixin modules and `_meta.lua` companions. |
| `Data/Blueprints` | Game-specific Actor Blueprint classes. |
| `Data/Materials` | Authored material graphs; `SceneInit` builds runtime shaders and metadata in the user data directory (templates ship source graphs, not caches). |
| `Data/General` | General Data databases for items, enemies, equipment, classes, players, states and special effects. |
| `Data/Maps`, `Data/Tilesets` | Playable maps and tileset definitions. |
| `Engine/` | Native C++ runtime, modules and dependencies. |

See [Build and Module Layout](<../../Native C++ Development/Build and Module Layout.md>) for native source ownership and dependencies.

## Template variants

Select a C++ Source or Standalone template following [Project Templates](<../../Getting Started/Project Templates.md>). Updates to the Game project do not modify existing projects automatically; apply changes per project or regenerate the template.

An existing C++ Source project can be [converted to Standalone](<../../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>) while preserving authored gameplay, declarations and current exports. See [Lua Runtime and Modules](<../Lua Runtime and Modules.md>) for Lua, `Scripts/stub` and `_meta.lua` ownership rules.
