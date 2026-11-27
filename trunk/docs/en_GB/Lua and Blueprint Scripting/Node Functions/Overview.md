# Node Functions Overview

This chapter lists the Lua Blueprint node libraries under `Scripts/GlobalFunctions/`, with their inputs, outputs, execution semantics and editor metadata.

## Modules

| Module | Responsibility | Page |
|---|---|---|
| `GlobalFunctions.Container` | Loop constructs and dictionary and list operations available as Blueprint nodes. | [Container.md](<Container.md>) |
| `GlobalFunctions.GameMap` | Actor lookup nodes. | [GameMap.md](<GameMap.md>) |
| `GlobalFunctions.Math` | Scalar, vector, rectangle, angle, random and operator nodes. | [Math.md](<Math.md>) |
| `GlobalFunctions.Mota` | Monster handbook, region and floor-transport nodes. | [Mota.md](<Mota.md>) |
| `GlobalFunctions.Movement` | Nodes that toggle movement, assign a movement route and start automatic pathing. | [Movement.md](<Movement.md>) |
| `GlobalFunctions.Player` | Nodes that read and change Player state, inventory and equipment. | [Player.md](<Player.md>) |
| `GlobalFunctions.Save` | Save, load and save-path nodes. | [Save.md](<Save.md>) |
| `GlobalFunctions.Scene` | Map transfer, dialogue, window, camera, terrain and persistence-record nodes. | [Scene.md](<Scene.md>) |
| `GlobalFunctions.Gameplay` | Nodes that read a gameplay event context and apply attributes, States and events. | [Gameplay.md](<Gameplay.md>) |
| `GlobalFunctions.String` | Conversion, search, case and substring nodes for string values. | [String.md](<String.md>) |
| `GlobalFunctions.Audio` | Nodes editing the shared `SoundFilter` and `MusicFilter` and playing audio; playback applies filters only when `applyFilter` is `true`. | [Audio.md](<Audio.md>) |
| `GlobalFunctions.Utils` | General-purpose nodes for flow control, local and game variables, attributes, animation helpers, the event bus and common functions. | [Utils.md](<Utils.md>) |
| `GlobalFunctions.Video` | The `PlayVideo` node; its `mute`, `skipable` and `subtitleFileName` arguments are optional. | [Video.md](<Video.md>) |
| `GlobalFunctions.Weather` | The `SetWeather` and `ClearWeather` nodes; the selector displays `NONE`, `RAIN`, `STORM` or `SNOW` and stores its `Enums.GlobalCore.WeatherType` value. | [Weather.md](<Weather.md>) |
| `GlobalFunctions.ScreenEffects` | Screen flash, tone and shake nodes, paired with stop or clear nodes. | [ScreenEffects.md](<ScreenEffects.md>) |
| `GlobalFunctions.Transition` | Freeze and transition-wait nodes returning `Engine.AsyncOperation`, shared by Blueprint execution and Lua `asyncio.await`. | [Transition.md](<Transition.md>) |

## Looking up a node

Choose a module by its purpose above, then look up the node's identifier. For example, the `+` node is `Math.ADD`.

The function tables on these pages use four columns:

- **`Function`** — the metadata identifier of the Blueprint node function.
- **`Parameters`** — ordered input pins as `name: type`, followed by `= value` when metadata declares a default, or `—` when the node takes no parameters.
- **`Returns`** — output pins as `name: type` (or bare `type` for unnamed return values), or `—` when the node produces no data output.
- **`Notes`** — execution flags (`Pure.`, `Latent.`, `Loop.`), named execution branches, editor metadata (drop-downs, path pickers, selectors, display names) and behaviour summaries. Non-pure, non-latent, non-loop nodes use a single default execution output unless branches are listed. See [Execution declarations](<../Blueprint Scripting/Metadata Schema and Decorators.md#execution-declarations>) for execution rules.

## Relationship to the GlobalFunctions Lua API

The native `GlobalFunctions` root exposes Lua APIs in `Components`, `Manager`, `NodeGraph` and `UI`; its empty metadata declares no Blueprint nodes. See [GlobalFunctions Lua API](<../Global and Core Modules/Core Modules/GlobalFunctions.md>).

The node libraries here load separately, for example `require("GlobalFunctions.Math")`, and are not members of that root table. Their loading and dependency rules are covered in [Lua Runtime and Modules](<../Lua Runtime and Modules.md#entry-and-module-order>).
