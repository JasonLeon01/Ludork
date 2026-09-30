# Global and Core Modules Overview

This group is the API reference for the three native Core modules that every Lua state loads with `require` — `Engine`, `GlobalCore` and `GlobalFunctions` — together with `Global`, the shared Lua module layer of the Game project. It is split by Lua root name rather than by topic because that root name decides how a member is called, whether it appears in Blueprint graphs, and which files declare it.

## The four names

`Engine`, `GlobalCore` and `GlobalFunctions` are C++20 modules built from `Engine/Source`. [Build and Module Layout](<../../Native C++ Development/Build and Module Layout.md>) lists their responsibilities and native dependency direction, and [Generated Metadata and Stubs](<../../Native C++ Development/Generated Metadata and Stubs.md>) describes the `Scripts/stub/<Module>.d.lua` declaration and `Scripts/<Module>_meta.lua` metadata that Core bindgen writes for each of them. `Global` is not native: it is ordinary project Lua under `Scripts/Global`, whose place in the project layout is described in [Project Settings and Asset Layout](<../../Editor User Guide/Project Settings and Asset Layout.md>).

| Name | What it is | Where it is defined | Pages in this group |
|---|---|---|---|
| `Engine` | The native root for engine types, services, state and gameplay foundations: `Actor`, `Character` and `Component`; curves and animation data; tile layers, tilemaps, autotiles and materials; input; the clock, render states, particles and emitters; the control, text and window classes; `EventBus` and the module root. `LudorkRuntime` exposes no Lua root of its own, so Runtime-owned values and Blueprint types register here as well. | C++ in `Engine/Source` and `Engine/Runtime`, loaded with `require("Engine")`; generated `Scripts/stub/Engine.d.lua` and `Scripts/Engine_meta.lua`. The Game project has no `Scripts/Engine/` Lua directory. | `Core Modules/Engine/`, 13 pages |
| `GlobalCore` | The native root for global game classes and services: `Camera` and the focus classes; `GameMapBase`, fog, panorama and world streaming; `Animation` and `Light`; the audio, font, shader and texture managers; `TimeManager`; `SceneBase`, `SceneManager`, `System`, `Display`, `Graphics`, `ScreenEffects` and `Transition`; `UIManager` and `WeatherController`; and the ability system. | C++ in `Engine/Source`, loaded with `require("GlobalCore")`; generated `Scripts/stub/GlobalCore.d.lua` and `Scripts/GlobalCore_meta.lua`. The Game project has no `Scripts/GlobalCore/` Lua directory. | `Core Modules/GlobalCore/`, 7 pages, plus [GlobalCore Gameplay API](<Global APIs/Gameplay.md>) |
| `GlobalFunctions` | The native root for free functions grouped into four tables: `Components`, `Manager`, `NodeGraph` and `UI`. `Scripts/GlobalFunctions_meta.lua` is empty, so these functions are Lua APIs rather than Blueprint nodes. | C++ in `Engine/Source/GlobalFunctions/include/GlobalFunctions`, loaded with `require("GlobalFunctions")`; generated `Scripts/stub/GlobalFunctions.d.lua`. | [GlobalFunctions Lua API](<Core Modules/GlobalFunctions.md>), 1 page |
| `Global` | The Game project's shared Lua modules, required with dotted paths such as `require("Global.GameMap")` and `require("Global.Utils.Logging")`. Static dependencies flow from the native roots through `Scripts/Global` to `Scripts/Source`, and Global modules must not require Source. | Lua in `Scripts/Global/`, with mirrored declarations under `Scripts/stub/Global/` and per-module metadata such as `Scripts/Global/GameMap_meta.lua`. | [Global.GameMap](<Global APIs/GameMap.md>), 1 page |

`Scripts/Global` also holds the root modules `ActorTree`, `Pool`, `Tutorial`, `WorldGameMap`, `WorldGeometry` and `WorldMapConstants`, the `Components`, `CustomEffects`, `CustomParticles` and `Utils` directories, and the same-name implementation directories of `GameMap.lua` and `WorldGameMap.lua`. `Global.GameMap` is the only one of them with a page in this group.

A fifth name collides with the third: the Lua Blueprint function libraries in `Scripts/GlobalFunctions/` load as separate modules, such as `require("GlobalFunctions.Math")`, and are not members of the native root table. They are documented in [Node Functions Overview](<../Node Functions/Overview.md>), not here.

### Two namespaces in `Global APIs`

The directory name does not name a namespace, and the H1s of its two pages are unrelated:

- [Global.GameMap](<Global APIs/GameMap.md>) documents the Lua class in `Scripts/Global/GameMap.lua`. Its `Metadata source:` line names `Scripts/Global/GameMap_meta.lua`, whose declared base is `{ "GlobalCore", "GameMapBase" }`: the project class extends a native `GlobalCore` type, and that native side is [GameMapBase](<Core Modules/GlobalCore/Map and Fog.md#gamemapbase>).
- [GlobalCore Gameplay API](<Global APIs/Gameplay.md>) documents the ability system that `require("GlobalCore")` exposes: `AttributeSet`, `AbilitySystemComponent`, and the ability, result, event, Effect and spec types. It belongs to the native module even though it sits outside `Core Modules/GlobalCore/`.

Read a page's H1, not its folder, to know which root you are calling. Inside the tables the type notation follows the metadata schema: a Core type is written `{ "Module", "Type" }`, as in `{ "Engine", "Actor" }` or `{ "GlobalCore", "Camera" }`, while a project Lua class carries its module path, as in `{ "Global.GameMap", "GameMap" }`. [Metadata Schema and Decorators](<../Blueprint Scripting/Metadata Schema and Decorators.md>) defines that notation and the `Pure` and `ExecSplit` entries of the `Execution and metadata` column.

## Directory map

The group holds 23 reference pages plus this overview, in three levels of nesting.

```text
Global and Core Modules/
├── Overview.md                 this page
├── Global APIs/                2 pages
└── Core Modules/
    ├── Engine/                13 pages
    ├── GlobalCore/             7 pages
    └── GlobalFunctions.md      1 page
```

| Sub-directory | Contents | Pages | First page |
|---|---|---|---|
| `Global APIs/` | Two pages that do not follow the module split: the project Lua class `Global.GameMap`, and the ability system exposed by `require("GlobalCore")`. | 2 | [Global.GameMap](<Global APIs/GameMap.md>) |
| `Core Modules/Engine/` | The bound classes of the `Engine` module in 13 topic pages, from `Scalar Curves.md` to `Interactive and Declarative UI Controls.md`. | 13 | [Engine: Scalar Curves](<Core Modules/Engine/Scalar Curves.md>) |
| `Core Modules/GlobalCore/` | The bound classes of the `GlobalCore` module in 7 topic pages, from `Camera and Focus.md` to `UI Weather and Module Root.md`. | 7 | [GlobalCore: Camera and Focus](<Core Modules/GlobalCore/Camera and Focus.md>) |
| `Core Modules/GlobalFunctions.md` | The native `GlobalFunctions` root: its four group tables, the order in which the three roots load, and the resource path rules of its loaders. | 1 | [GlobalFunctions Lua API](<Core Modules/GlobalFunctions.md>) |

The Engine and GlobalCore pages list every bound class with its `Direct metadata bases`, its editable properties, and its functions and events with their Blueprint execution metadata. Some of them open with a prose rule that applies to every class on the page, such as the resource path rules of [Engine: Maps and Materials](<Core Modules/Engine/Maps and Materials.md>) and [GlobalCore: Resource Managers](<Core Modules/GlobalCore/Resource Managers.md>).

## Which page do I need

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

## Reading order

1. [Lua Runtime and Modules](<../Lua Runtime and Modules.md>) — the required load order of the three native roots, the `.lua`, `stub/**/*.d.lua` and `_meta.lua` file layers, and the `Global` → `Source` dependency direction.
2. [GlobalFunctions Lua API](<Core Modules/GlobalFunctions.md>) — a single page: the four group tables and the category-relative path rules of its loaders.
3. [Engine: Gameplay Types](<Core Modules/Engine/Gameplay Types.md>) — `Actor`, `Character` and `Component`, the types that appear in the parameter and return columns of most other pages.
4. [GlobalCore: Scenes and System](<Core Modules/GlobalCore/Scenes and System.md>) — `SceneBase`, `SceneManager`, `System`, `Display` and `Graphics`: the services that own a running game.
5. [Global.GameMap](<Global APIs/GameMap.md>) together with [GameMapBase](<Core Modules/GlobalCore/Map and Fog.md#gamemapbase>) — how a project Lua class extends a native `GlobalCore` class, and how the two pages divide the map API.
6. [GlobalCore: Camera and Focus](<Core Modules/GlobalCore/Camera and Focus.md>) — the map's view and its focus navigation.

The remaining Engine and GlobalCore pages are reference tables rather than a narrative, so read them by topic from the list above. Take [GlobalCore Gameplay API](<Global APIs/Gameplay.md>) when a project uses attributes, abilities and Effects.

## Related pages

- [Lua Runtime and Modules](<../Lua Runtime and Modules.md>)
- [Node Functions Overview](<../Node Functions/Overview.md>)
- [GlobalFunctions Lua API](<Core Modules/GlobalFunctions.md>)
- [Build and Module Layout](<../../Native C++ Development/Build and Module Layout.md>)
- [Generated Metadata and Stubs](<../../Native C++ Development/Generated Metadata and Stubs.md>)
