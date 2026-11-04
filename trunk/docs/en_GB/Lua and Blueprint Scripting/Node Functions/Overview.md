# Node Functions Overview

This chapter lists the Lua Blueprint nodes under `GlobalFunctions`, with their inputs, outputs and execution rules.

## Modules

| Module | Responsibility | Page |
|---|---|---|
| `GlobalFunctions.Container` | Loop constructs and the dictionary and list operations available as Blueprint nodes. | [Container.md](<Container.md>) |
| `GlobalFunctions.GameMap` | Actor lookup nodes. | [GameMap.md](<GameMap.md>) |
| `GlobalFunctions.Math` | Scalar, vector, angle and operator nodes. | [Math.md](<Math.md>) |
| `GlobalFunctions.Mota` | Monster-book, region and floor-transport nodes. | [Mota.md](<Mota.md>) |
| `GlobalFunctions.Movement` | Nodes that toggle movement, set a movement route and start automatic pathing. | [Movement.md](<Movement.md>) |
| `GlobalFunctions.Player` | Nodes that read and change Player state, inventory and equipment. | [Player.md](<Player.md>) |
| `GlobalFunctions.Save` | Save, load and save-path nodes. | [Save.md](<Save.md>) |
| `GlobalFunctions.Scene` | Map transfer, dialogue, window, camera, terrain and persistence-record nodes. | [Scene.md](<Scene.md>) |
| `GlobalFunctions.Gameplay` | Nodes that read a gameplay event context and apply attributes, States and events. | [Gameplay.md](<Gameplay.md>) |
| `GlobalFunctions.String` | Conversion, search, case and substring nodes for string values. | [String.md](<String.md>) |
| `GlobalFunctions.Audio` | Nodes editing the shared `SoundFilter` and `MusicFilter`; playback applies them only when `applyFilter` is true. | [Audio.md](<Audio.md>) |
| `GlobalFunctions.Utils` | General-purpose nodes for flow control, local and game variables, attributes, animation helpers, the event bus and common functions. | [Utils.md](<Utils.md>) |
| `GlobalFunctions.Video` | The `PlayVideo` node; its `mute`, `skipable` and `subtitleFileName` arguments are optional. | [Video.md](<Video.md>) |
| `GlobalFunctions.Weather` | The `SetWeather` and `ClearWeather` nodes; the selector shows `NONE`, `RAIN`, `STORM` or `SNOW` and stores its `Enums.GlobalCore.WeatherType` value. | [Weather.md](<Weather.md>) |
| `GlobalFunctions.ScreenEffects` | Screen flash, tone and shake nodes, with the calls that stop or clear them. | [ScreenEffects.md](<ScreenEffects.md>) |
| `GlobalFunctions.Transition` | Freeze and transition-wait nodes returning `Engine.AsyncOperation`, shared by Blueprint execution and Lua `asyncio.await`. | [Transition.md](<Transition.md>) |

## Looking up a node

Choose a module by its purpose above, then look up the node identifier. For example, the `+` node is `Math.ADD`.

The function tables on those pages use these columns:

- `Name` — the metadata identifier of the function or event.
- `Kind` — the declared metadata `type`, such as `function` or `event`.
- `Parameters` — the ordered input pins as `name: type`, followed by `= value` when the metadata declares a default, or `—` when the node takes no parameters.
- `Returns` — the output pins in the same form, or `—` when the declaration has no outputs.
- `Execution and metadata` — the `Pure`, `ExecSplit`, `Latent`, `LatentStates`, `LoopNode` and `Meta` declarations. [Execution declarations](<../Blueprint Scripting/Metadata Schema and Decorators.md#execution-declarations>) explains their execution rules.

## Relationship to the GlobalFunctions Lua API

The native `GlobalFunctions` root exposes Lua APIs in `Components`, `Manager`, `NodeGraph` and `UI`; its empty metadata declares no Blueprint nodes. See [GlobalFunctions Lua API](<../Global and Core Modules/Core Modules/GlobalFunctions.md>).

The node libraries here load separately, for example `require("GlobalFunctions.Math")`, and are not members of that root table. Their loading and dependency rules are covered in [Lua Runtime and Modules](<../Lua Runtime and Modules.md#entry-and-module-order>).

## Related pages

- [Execution Flow, Events and Variables](<../Blueprint Scripting/Execution Flow Events and Variables.md>)
