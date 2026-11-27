# Source Classes Overview

The `Source.*` Lua classes belong to the Default Gameplay template under `Scripts/Source/`. Use the table below to look up each class's inheritance, responsibilities, properties, methods and events.

## Classes

| Class | Inherits | Responsibility | Page |
|---|---|---|---|
| `Source.Battler` | — | Lua gameplay base of `Enemy` and `Player`, which each construct a generated Attribute Set from General Data | [Battler.md](<Battler.md>) |
| `Source.Components.ChildActorComponent` | `Engine.Component` | Declares a class name and a relative position for a child Actor | [ChildActorComponent.md](<ChildActorComponent.md>) |
| `Source.MapActors.DoorBase` | `Source.MapActors.ConditionalActor` | Declares door collision, opening interval, gate sound and door state properties, together with the open and close nodes | [DoorBase.md](<DoorBase.md>) |
| `Source.MapActors.Enemy` | `Source.MapActors.ConditionalActor`, `Source.Battler` | Conditionally visible Battler Actor bound to an Enemy General Data `ID`, with a damage-text child component and collision and defeat events | [Enemy.md](<Enemy.md>) |
| `Source.MapActors.EnemyDamageText` | `Engine.Actor` | Declares the required Item, text configuration and offset of the Enemy damage readout | [EnemyDamageText.md](<EnemyDamageText.md>) |
| `Source.MapActors.Equip` | `Source.MapActors.ConditionalActor` | Declares the General Data identifier and pickup sound effect of an equipment Actor | [Equip.md](<Equip.md>) |
| `Source.MapActors.Item` | `Source.MapActors.ConditionalActor` | Conditionally visible pickup Actor that builds an Item Attribute Set from `ID` and grants `count` Items to the Player on collision | [Item.md](<Item.md>) |
| `Source.MapActors.Player` | `Engine.Character`, `Source.Battler` | Declares the Character properties and the inventory, equipment and attribute commands of the playable Actor | [Player.md](<Player.md>) |
| `Source.Gameplay` | Varies by module | Game project gameplay Scene, Actor, ability and Effect modules built on `GlobalCore` | [Gameplay.md](<Gameplay.md>) |
| `Source.Scenes.SceneMap` | `Source.Gameplay.GameplayScene` | Declares the gameplay commands, dialogue windows, menu entry points and persistence records of a map Scene | [SceneMap.md](<SceneMap.md>) |
| `Source.MapActors.Teleporter` | `Source.MapActors.ConditionalActor` | Declares stair direction, offset, sound, transition and timing properties, together with stair-transfer and chosen-map nodes | [Teleporter.md](<Teleporter.md>) |
| `Source.MapActors.ConditionalActor` | `Engine.Actor` | Declares the condition variable, operator and comparison value that control Actor visibility | [ConditionalActor.md](<ConditionalActor.md>) |
