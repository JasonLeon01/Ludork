# New Game, Title and Scene Flow

This page describes how Default Gameplay moves from startup (`SceneInit`) through the title screen into ordinary maps and composite worlds.

The default entry creates `Source.Scenes.SceneInit` and hands control to the native System loop. `SceneInit` prepares shared data and caches with a loading bar, then transitions to the title screen. The title command window fits its visible options: **Start Game**, **Continue** and **Config** on [WebGL](<../../Editor User Guide/Run Debug and Package.md#webgl>), plus **Exit Game** on native platforms. Its Save/Load and Config windows construct on first use and are reused until the title scene is destroyed.

Scene transitions use `GlobalCore.SceneManager`, screen freezes and transition masks use `GlobalCore.Transition`, and flash, shake and tone effects use `GlobalCore.ScreenEffects`.

## Configure New Game

Starting a new game creates or resets the game instance, loads the initial map and player data, and enters the map scene.

- **Player and region**: `Data/Configs/System.json` specifies `startPlayerBlueprint` (relative to `Data/Blueprints`) and `startRegion`. Both apply only to new games; **Continue** restores the player class and current region from the save slot.
- **Initial map and position**: `startMap` may target an ordinary map, a composite-world manifest (such as `World_01/_world.json`) or a placed world child (such as `World_01/Map_03.json`). `startPos` uses world coordinates for a manifest target and child-local coordinates for a child target. The editor displays all three forms in one map tree and stores the selected path.

See [Windows, Menus, Input and Controls](<Windows Menus Input and Controls.md>) for map-window creation and reuse.

## Enter maps and composite worlds

`GlobalFunctions.Scene.GotoMap` follows the same target path and coordinate rules as `startMap`. Moving to another placed child inside the active world reuses its `WorldGameMap` facade; unplaced children are invalid destinations.

### World facade and entry validation

- **Unified world view**: `Scene:getGameMap()` and `Actor:getMap()` expose a single world-coordinate `GameMap` facade. Child placements supply passability, Dir4 edges, materials and terrain; uncovered cells act as passable holes. Missing or inconsistent child data raises an error.
- **World display name**: The facade reads `mapName` from the required `worldName` field in `_world.json`, so UI such as the player HUD names the whole world rather than a streamed child. Child maps keep their own `mapName` for standalone loading.
- **Revalidation**: On entry, the runtime revalidates the flat world directory, exact `_world.json` filename, required `worldName`, direct JSON child files, placements and layer order. Unrelated non-JSON regular files are ignored, and invalid data fails loading.

### Streaming, simulation and transitions

- **Child lifecycle**: Streamed children progress through `Unloaded`, `Reading`, `Prepared`, `Active` and `Dormant`. Streaming prioritises the Camera neighbourhood, cancels obsolete reads, and prepares tilesets and autotiles through the [region builder's asynchronous resource requests](<../Global and Core Modules/Global APIs/GameMap.md#map-data-and-region-builders>).
- **Simulation and readiness**: Only `Active` Actors simulate (including `Active` Actors outside the Camera view, which simulate without rendering). Until all visible placements and tile chunks are ready, the Camera holds its last complete frame and colliding movement cannot enter the unprepared region.
- **Entry transitions**: With transitions enabled, world entry freezes the outgoing frame until the target viewport and its required Actors, lights and tile chunks are ready; required-data errors propagate before releasing the freeze. Setting `blockTransition = true` skips the transition mask while keeping the readiness barrier. Surrounding streaming runs asynchronously.
- **Floor transfer data**: `RegionDict` defines floor-transfer order only; it does not describe composite-world layout or streaming.

### Map assembly and persistent state

`Source.SceneComponents.MapBuilder` owns project map assembly and supplies the world region factory, while shared map data types and the `Global.WorldGameMap.RegionBuildState` interface belong to `Global` (see [Map data and region builders](<../Global and Core Modules/Global APIs/GameMap.md#map-data-and-region-builders>)).

Ordinary map loading and adjacent-floor passability previews call `MapBuilder:applyPersistentState(gameMap, inst, mapFile)`. It restores terrain edits, added Actors, recorded positions and destroyed-Actor removals in that order without invoking Actor `onCreate`; the live Scene starts surviving Actors after binding its map and player. See [SceneMap](<../Source Classes/SceneMap.md#scene>) for Scene and component ownership.

## Configure Continue

When the title command window is first constructed, it checks `Engine.getSavePath()`:

- If the directory contains any direct entry (even an unrelated file or subdirectory), **Continue** (`index = 1`) is preselected.
- If the directory is missing or empty, **Start Game** (`index = 0`) is preselected.

This check does not parse save files or filter by format. Locale refreshes and returning from Config or Save/Load preserve the active selection; constructing a new title window checks the directory again.

**Continue** reads the plain or encrypted save selected by the build via `GetSavePath(slot)`, restores game state and opens the saved scene. Missing, malformed or unsupported saves follow the explicit not-found or error path rather than building partial state.

## Preserve scene transitions

Change scenes through the documented System and Scene APIs. The C++ runtime schedules scene switches between updates so active ticks are never torn down mid-frame, and outgoing windows and temporary resources release cleanly through the scene lifecycle.

## Customise the title

Edit the title Lua scene, window composition and JSON assets to match your project, keeping all text keys aligned with [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>).

> **Note:** Default Gameplay does not impose a universal save-upgrade policy or title layout. Define supported save versions, presentation and error recovery messages for your release.
