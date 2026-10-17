# New Game, Title and Scene Flow

The default entry creates `Source.Scenes.SceneInit` and then hands control to the native System loop. SceneInit prepares shared data and advances to the title flow. Loading progress appears in the initialisation screen. The title scene creates the command window for Start Game, Continue, Config and Exit Game. Save/Load and Config are created on first use and reused until the title scene is destroyed.

Scene changes use `GlobalCore.SceneManager`; freezing and transition requests use `GlobalCore.Transition`. Screen flash, shake and tone use `GlobalCore.ScreenEffects`.

### Configure New Game

A new-game action creates or resets the game instance, selects the initial map and player data, and changes to the map scene. `Data/Configs/System.json` stores the player Blueprint under `startPlayerBlueprint` as a path relative to `Data/Blueprints`, and stores the initial region under `startRegion`. These values initialise new games only. Continue restores the player class and the current region saved in the selected slot.

Map entry creates the HUD and Region Title, while optional popups and dialogue wait for their first display. This keeps unused window construction out of scene entry without changing transition durations. Window ownership and reuse are described in [Windows, Menus, Input and Controls](<Windows Menus Input and Controls.md>).

`startMap` may identify an ordinary map, a composite-world manifest such as `World_01/_world.json`, or a placed world child such as `World_01/Map_03.json`. `startPos` is a world coordinate for a manifest target and a child-local coordinate for a child target. The editor presents all three forms through one map tree and stores the selected string path.

### Enter maps and composite worlds

`GlobalFunctions.Scene.GotoMap` follows the same path and coordinate rules. Entering another child of the current world keeps its `WorldGameMap` facade. Unplaced children are invalid destinations.

For a world, `Scene:getGameMap()` and `Actor:getMap()` expose one world-coordinate GameMap facade. Child placements provide passability, Dir4, materials and terrain. Uncovered cells are passable holes. Missing or inconsistent child data is an error.

The facade's `mapName` comes from the required `worldName` in `_world.json`, so UI such as the player HUD identifies the complete world rather than an individual streamed child. Child map JSON retains its own `mapName` for standalone use, but the HUD does not display that name while the child belongs to a loaded world.

On entry, the runtime revalidates the flat world directory, the exact `_world.json`, the required world name, direct JSON child files, placements and layer order. Unrelated non-JSON regular files are ignored, and invalid data fails.

Children progress through `Unloaded`, `Reading`, `Prepared`, `Active` and `Dormant`. Streaming prioritises the Camera neighbourhood and cancels obsolete reads. Only Active Actors simulate. Until every visible placement and Tile chunk is ready, the Camera retains its last complete frame, and colliding movement cannot enter the unprepared area.

Active Actors outside the Camera view continue to simulate without rendering.

With transitions enabled, world entry freezes the outgoing frame until the target viewport and the required Actors, lights and Tile chunks are ready. Required-data failures propagate before release. `blockTransition = true` skips the mask but keeps this readiness barrier. The surrounding preparation remains asynchronous.

`RegionDict` remains floor-transfer data and does not define world composition or streaming.

`Source.SceneComponents.MapBuilder` owns project map assembly and supplies the world region factory. Shared map data types and the `Global.WorldGameMap.RegionBuildState` interface belong to Global; project construction state remains in Source. See [Map data and region builders](<../Global and Core Modules/Global APIs/GameMap.md#map-data-and-region-builders>) when implementing a custom builder.

### Configure Continue

When the title command window is first created, it checks the directory returned by `Engine.getSavePath()`. Any direct entry, including an unrelated file or a subdirectory, selects Continue (`index = 1`); a missing or empty directory selects Start Game (`index = 0`). This check does not parse saves or filter their format. Locale refreshes and returning from Config or Save/Load preserve the current selection. Creating a new title window checks the directory again.

Continue uses `GetSavePath(slot)` to read the plain or encrypted save selected by the native build. It restores game state and opens the appropriate scene. A missing, malformed or unsupported save must follow the explicit not-found or error path instead of silently constructing partial state.

### Preserve scene transitions

Use the System and scene APIs documented in the API reference. The C++ runtime schedules scene changes, so active updates are not destroyed midway through a frame. Windows and temporary resources owned by the outgoing scene are released through its normal lifecycle.

### Customise the title

Edit the Lua scene and window composition and the corresponding data. Keep all text identifiers compatible with [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>).

## Limitations

The Game project does not define a universal save upgrade policy or title-screen design. A released game must define the versions it supports and provide its own presentation and recovery messages.

## Related pages

- [Execution Flow, Events and Variables](<../Blueprint Scripting/Execution Flow Events and Variables.md>)
- [Tilesets, Autotiles and Maps](<../../Editor User Guide/Tilesets Autotiles and Maps.md>)
- [GlobalCore: Scenes and System](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md>)
- [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>)
