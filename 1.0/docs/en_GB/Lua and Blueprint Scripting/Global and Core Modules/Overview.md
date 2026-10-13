# Global and Core Modules Overview

This chapter covers the three native Lua modules and the shared Lua modules in Default Gameplay.

| Module | Purpose |
|---|---|
| `Engine` | Actors, components, input, graphics, data and UI controls. |
| `GlobalCore` | Scenes, maps, cameras, resource managers and the ability system. |
| `GlobalFunctions` | Lua helpers grouped into `Components`, `Manager`, `NodeGraph` and `UI`. |
| `Global.*` | Shared Game project modules, such as `Global.GameMap`. |

Load native modules with names such as `require("Engine")` and project modules by their full paths, such as `require("Global.GameMap")`. [Lua node libraries](<../Node Functions/Overview.md>) such as `GlobalFunctions.Math` load separately and are not members of the native `GlobalFunctions` root table. See [Lua Runtime and Modules](<../Lua Runtime and Modules.md>) for loading order.

## Which page do I need

- Optional remote account dictionaries, one-use write tokens and connection probes: [Simple Server native API](<../../Simple Server/Native Client API.md>). `GlobalCore.Server`, its typed results, setup and lifecycle are documented together there.
- Curves, animation data and animation playback: [Engine: Scalar Curves](<Core Modules/Engine/Scalar Curves.md>), [Engine: Vector Curves](<Core Modules/Engine/Vector Curves.md>), [Engine: Animation Data and Graphics](<Core Modules/Engine/Animation Data and Graphics.md>) and [GlobalCore: Animation and Light](<Core Modules/GlobalCore/Animation and Light.md>).
- Actors, characters and components: [Engine: Gameplay Types](<Core Modules/Engine/Gameplay Types.md>).
- Tile layers, tilemaps, autotiles, tilesets and materials: [Engine: Maps and Materials](<Core Modules/Engine/Maps and Materials.md>).
- Map services, fog, panorama, path results and world streaming: [GlobalCore: Map and Fog](<Core Modules/GlobalCore/Map and Fog.md>). The project's own map class, with its Actor queries, terrain edits and map lights, is [Global.GameMap](<Global APIs/GameMap.md>).
- Audio filters and sound playback: [Engine: Audio Filters](<Core Modules/Engine/Audio Filters.md>) for `SoundFilter` and `MusicFilter`, the `Manager` group of [GlobalFunctions Lua API](<Core Modules/GlobalFunctions.md>) for `playSE`, `playVoice` and `playMusic`, and [Node Functions Overview](<../Node Functions/Overview.md>) for the Blueprint nodes.
- Keyboard, mouse, touch, joystick and input capture: [Engine: Input and Services](<Core Modules/Engine/Input and Services.md>).
- Frame clock, render states, particles and emitters: [Engine: Time, Rendering and Particles](<Core Modules/Engine/Time Rendering and Particles.md>).
- Timers and time management: [GlobalCore: Time Management](<Core Modules/GlobalCore/Time Management.md>).
- Layout, image, text, window and interactive controls: [Engine: Runtime Values and Functional Controls](<Core Modules/Engine/Runtime Values and Functional Controls.md>), [Engine: Layout and Image Controls](<Core Modules/Engine/Layout and Image Controls.md>), [Engine: Text and Windows](<Core Modules/Engine/Text and Windows.md>) and [Engine: Interactive and Declarative UI Controls](<Core Modules/Engine/Interactive and Declarative UI Controls.md>).
- Camera, focus navigation and common tips: [GlobalCore: Camera and Focus](<Core Modules/GlobalCore/Camera and Focus.md>).
- Scenes, system services, display, graphics, screen effects and transitions: [GlobalCore: Scenes and System](<Core Modules/GlobalCore/Scenes and System.md>).
- `UIManager`, weather and the `GlobalCore` module root: [GlobalCore: UI, Weather and Module Root](<Core Modules/GlobalCore/UI Weather and Module Root.md>).
- Attributes, abilities, Effects and gameplay tags: [GlobalCore Gameplay API](<Global APIs/Gameplay.md>).
- Texture, font, shader and sound loading, and their `/Game/Assets/...` path rules: [GlobalCore: Resource Managers](<Core Modules/GlobalCore/Resource Managers.md>).
- `EventBus`, JSON data, resource-file constants, save previews and the `Engine` module root: [Engine: Events and Module Root](<Core Modules/Engine/Events and Module Root.md>).
- Component metadata and serialisation helpers, and the colour helpers: [GlobalFunctions Lua API](<Core Modules/GlobalFunctions.md>).
