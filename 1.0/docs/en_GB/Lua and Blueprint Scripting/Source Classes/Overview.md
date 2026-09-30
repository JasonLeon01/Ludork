# Source Classes Overview

The `Source.*` classes are the Lua gameplay classes of the Default Gameplay project template, each exposed to Blueprint through its editor metadata. They live under `Scripts/Source/` in the Game project, and every page in this section names the class's same-name `*_meta.lua` file on its `Metadata source:` line; its property, function and event tables list what that metadata declares.

```text
Scripts/Source/
├── Battler.lua                Battler_meta.lua
├── Components/
│   └── ChildActorComponent.lua                ChildActorComponent_meta.lua
├── MapActors/
│   └── ConditionalActor, DoorBase, Enemy, EnemyDamageText,
│       Equip, Item, Player, Teleporter        (each .lua + _meta.lua)
└── Scenes/
    └── SceneMap.lua           SceneMap_meta.lua
```

`Scripts/Source/` also contains the `Configs/`, `Data/`, `Gameplay/`, `Locale/`, `SceneComponents/`, `Utils/` and `Windows/` directories and the root modules `Data.lua`, `GameInstance.lua`, `Save.lua` and `System.lua`. Those modules are not class pages of this section. A same-name `_meta.lua` file contains pure editor metadata and never runs as a gameplay script; see [Lua Runtime and Modules](<../Lua Runtime and Modules.md#runtime-declarations-and-metadata>).

## Namespaces

The class pages in this section use four namespaces. Each namespace matches the directory that holds the class and its metadata file.

| Namespace | Directory | What it groups |
|---|---|---|
| `Source` | `Scripts/Source/` | The root module layer. `Source.Battler` is the only class page here, shared by the two combat Actors. |
| `Source.Components` | `Scripts/Source/Components/` | Reusable Actor components. Contains `ChildActorComponent`. |
| `Source.MapActors` | `Scripts/Source/MapActors/` | Map Actor classes. The largest group: `ConditionalActor`, `DoorBase`, `Enemy`, `EnemyDamageText`, `Equip`, `Item`, `Player` and `Teleporter`. |
| `Source.Scenes` | `Scripts/Source/Scenes/` | Scene classes. `SceneMap` is the only class page here. |

These pages also refer to `Source.Gameplay`, `Source.Configs` and `Source.Utils` modules, which [Source Gameplay API](<Gameplay.md>) documents instead, and to `Source.GameInstance` and `Source.Windows`, which have no class page in this section.

## Classes

Responsibilities are one-line summaries; each page owns the full property, function and event reference.

| Class | Direct metadata base(s) | Responsibility | Page |
|---|---|---|---|
| `Source.Battler` | none declared | Lua gameplay base of `Enemy` and `Player`, which construct their generated Attribute Set from General Data | [Battler.md](<Battler.md>) |
| `Source.Components.ChildActorComponent` | `{ "Engine", "Component" }` | Declares a class name and a relative position for a child Actor | [ChildActorComponent.md](<ChildActorComponent.md>) |
| `Source.MapActors.DoorBase` | `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }` | Declares the door collision, opening interval, gate sound and door state properties, together with the open and close nodes | [DoorBase.md](<DoorBase.md>) |
| `Source.MapActors.Enemy` | `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`, `{ "Source.Battler", "Battler" }` | Conditionally visible Battler Actor bound to an Enemy General Data `ID`, with a damage-text child component and collision and defeat events | [Enemy.md](<Enemy.md>) |
| `Source.MapActors.EnemyDamageText` | `{ "Engine", "Actor" }` | Declares the required Item, text configuration and offset of the Enemy damage readout | [EnemyDamageText.md](<EnemyDamageText.md>) |
| `Source.MapActors.Equip` | `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }` | Declares the General Data identifier and the sound effect of an equipment Actor | [Equip.md](<Equip.md>) |
| `Source.MapActors.Item` | `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }` | Conditionally visible pickup Actor that builds an Item Attribute Set from `ID` and grants `count` Items to the Player on collision | [Item.md](<Item.md>) |
| `Source.MapActors.Player` | `{ "Engine", "Character" }`, `{ "Source.Battler", "Battler" }` | Declares the Character properties and the inventory, equipment and attribute commands of the playable Actor | [Player.md](<Player.md>) |
| `Source.Scenes.SceneMap` | `{ "Source.Gameplay.GameplayScene", "GameplayScene" }` | Declares the gameplay commands, dialogue windows, menu entry points and persistence records of a map Scene | [SceneMap.md](<SceneMap.md>) |
| `Source.MapActors.Teleporter` | `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }` | Declares the stair offset, sound, transition and timing properties, together with stair-transfer and chosen-map nodes | [Teleporter.md](<Teleporter.md>) |
| `Source.MapActors.ConditionalActor` | `{ "Engine", "Actor" }` | Declares the condition variable, operator and comparison value that control Actor visibility | [ConditionalActor.md](<ConditionalActor.md>) |

## Inheritance shape

Read from the `Direct metadata base(s)` column alone:

- Four classes root in `Engine`: `ConditionalActor` and `EnemyDamageText` in `{ "Engine", "Actor" }`, `Player` in `{ "Engine", "Character" }` and `ChildActorComponent` in `{ "Engine", "Component" }`.
- `Source.MapActors.ConditionalActor` is the most reused base in the group. `DoorBase`, `Enemy`, `Equip`, `Item` and `Teleporter` each list it as a direct metadata base.
- `Source.Battler` is the second shared base. `Enemy` and `Player` list it, and these are the only two classes in the group that declare two direct metadata bases. `Battler.md` itself carries no `Direct metadata base:` line, and `Battler_meta.lua` declares no `bases`.
- `SceneMap` is the only class whose base comes from outside this group: `Source.Gameplay.GameplayScene`, documented in [Source Gameplay API](<Gameplay.md#gameplayscene>).
- No class in the group lists `EnemyDamageText` or `ChildActorComponent` as a direct metadata base. `Enemy` references `ChildActorComponent` as the type of its `childActorComp` property.

### Inherited visibility conditions

`DoorBase.md`, `Enemy.md`, `Equip.md`, `Item.md` and `Teleporter.md` each carry an `Inherited visibility conditions:` line linking to `ConditionalActor.md`. The line records that these classes do not declare visibility properties of their own: the condition variable, operator and comparison value come from `ConditionalActor`, which continuously drives `setVisible(result, false)` and subscribes to `GameInstance:getVariables()` in `onCreate()`. The Inheritance section of `ConditionalActor.md` names the same five classes as its inheritors, and notes that `EnemyDamageText` retains its existing parent — which is why that page shows `{ "Engine", "Actor" }` and no visibility line.

## Reading order

For a first read of the Default Gameplay template, start with the project shape in [Default Template Overview](<../Default Gameplay/Default Template Overview.md>), then take the class pages in this order:

1. [Source.MapActors.ConditionalActor](<ConditionalActor.md>) — the shared base of five map Actors, and the visibility rule they all inherit.
2. [Source.Battler](<Battler.md>) — the shared combat base and its attribute accessors.
3. [Source.MapActors.Player](<Player.md>) then [Source.MapActors.Enemy](<Enemy.md>) — the two Battlers, in that order because Enemy combat resolves against the Player.
4. [Source.MapActors.Item](<Item.md>) and [Source.MapActors.Equip](<Equip.md>) — the pickups that feed the Player inventory and equipment.
5. [Source.MapActors.DoorBase](<DoorBase.md>) and [Source.MapActors.Teleporter](<Teleporter.md>) — map traversal and its latent nodes.
6. [Source.Components.ChildActorComponent](<ChildActorComponent.md>) and [Source.MapActors.EnemyDamageText](<EnemyDamageText.md>) — the supporting component and readout Actor.
7. [Source.Scenes.SceneMap](<SceneMap.md>) with [Source Gameplay API](<Gameplay.md>) — the Scene that owns the map, the records and the transfer requests.

## Related pages

- [Source Gameplay API](<Gameplay.md>)
- [Default Template Overview](<../Default Gameplay/Default Template Overview.md>)
- [Actors, Enemies, Items and Equipment](<../Default Gameplay/Actors Enemies Items and Equipment.md>)
- [Execution Flow, Events and Variables](<../Blueprint Scripting/Execution Flow Events and Variables.md>)
