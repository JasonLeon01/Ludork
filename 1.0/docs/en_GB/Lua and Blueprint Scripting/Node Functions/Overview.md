# Node Functions Overview

Each page in this chapter documents one Blueprint node module under the `GlobalFunctions` namespace: the nodes that module makes available to Blueprint graphs. Every page names its metadata source on its `Metadata source:` line — a `Scripts/GlobalFunctions/<Name>_meta.lua` file — and its function tables list what that metadata declares.

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
| `GlobalFunctions.Weather` | The `SetWeather` and `ClearWeather` nodes; the weather type is one of `NONE`, `RAIN`, `STORM` or `SNOW`. | [Weather.md](<Weather.md>) |
| `GlobalFunctions.ScreenEffects` | Screen flash, tone and shake nodes, with the calls that stop or clear them. | [ScreenEffects.md](<ScreenEffects.md>) |
| `GlobalFunctions.Transition` | Freeze and transition-wait nodes; their callable wait conditions live in `GlobalFunctions.FrozenCondition` and `GlobalFunctions.TransitionCondition`. | [Transition.md](<Transition.md>) |

## Looking up a node

Start from the node in the Blueprint graph, recover its metadata identifier, then use the responsibilities above to choose a module page and read that identifier's row.

The editor derives a node's displayed label from its identifier by splitting camel case, Pascal case and underscore boundaries and capitalising each displayed word, so a `Name` such as `GetActorByTag` appears in the graph as separated, capitalised words. Operator nodes are the exception: they may carry a literal `Meta.DisplayName`, which is why the `+` node is `Math.ADD` and the `+=` node is `Math.IADD`. See [Editor names and decorators](<../Blueprint Scripting/Metadata Schema and Decorators.md#editor-names-and-decorators>).

The function tables on those pages use these columns:

- `Name` — the metadata identifier of the function or event.
- `Kind` — the declared metadata `type`, such as `function` or `event`. A few pages, including [GlobalFunctions.Gameplay](<Gameplay.md>), omit this column.
- `Parameters` — the ordered input pins as `name: type`, followed by `= value` when the metadata declares a default, or `—` when the node takes no parameters.
- `Returns` — the output pins in the same form, or `—` when the declaration has no outputs.
- `Execution and metadata` — the `Pure`, `ExecSplit`, `Latent`, `LatentStates`, `LoopNode` and `Meta` entries copied from the metadata. [Execution declarations](<../Blueprint Scripting/Metadata Schema and Decorators.md#execution-declarations>) explains what each one promises to the Blueprint runtime.

Some pages add prose after the table for nodes whose behaviour the columns cannot express.

## Relationship to the GlobalFunctions Lua API

These modules are not the native `GlobalFunctions` Lua root. That root exposes native free functions through the `Components`, `Manager`, `NodeGraph` and `UI` group tables, and `Scripts/GlobalFunctions_meta.lua` is empty, so those functions are Lua APIs rather than Blueprint nodes. A grouped function appears only in its group table. See [GlobalFunctions Lua API](<../Global and Core Modules/Core Modules/GlobalFunctions.md>).

The modules documented here are the Lua Blueprint function libraries in `Scripts/GlobalFunctions`. They load as separate modules, such as `require("GlobalFunctions.Math")`, and are not added to the native root table. Module loading order and the `require` restrictions that apply to `Scripts/GlobalFunctions` are covered in [Lua Runtime and Modules](<../Lua Runtime and Modules.md>).

## Related pages

- [GlobalFunctions Lua API](<../Global and Core Modules/Core Modules/GlobalFunctions.md>)
- [Execution Flow, Events and Variables](<../Blueprint Scripting/Execution Flow Events and Variables.md>)
- [Lua Runtime and Modules](<../Lua Runtime and Modules.md>)
