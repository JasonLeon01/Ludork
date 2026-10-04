# ChildActor Component

## Purpose

Create a child Actor from a class reference and place it relative to its owner. Default Gameplay uses it for the enemy damage readout.

## Applicability

Default Gameplay `Source.MapActors.Enemy` declares `childActorComp` as `Source.Components.ChildActorComponent`; Enemy Blueprints and their descendants inherit it. Native `Engine.Actor` does not declare this field, so it is not available on every Actor.

## Configuration

1. Open an Enemy Blueprint or select one of its map instances, then open `childActorComp` in **Components** using the [component workflow](<Overview.md#configuration>).
2. Set `className` to the full Actor class reference. Enemy defaults to `Source.MapActors.EnemyDamageText`; a Blueprint child uses a reference such as `Data.Blueprints.Actors.Guard`. An empty class name creates no child.
3. Set `relativePosition` for the child's local position relative to the owner; the default is `[0,0]`.
4. Save the Blueprint or map. Keep the default damage-readout class when that is the intended enemy behaviour.

## References

- [Actor Components](<Overview.md>)
- [ChildActorComponent API](<../../Lua and Blueprint Scripting/Source Classes/ChildActorComponent.md>)
- [Enemy API](<../../Lua and Blueprint Scripting/Source Classes/Enemy.md>)
- [EnemyDamageText API](<../../Lua and Blueprint Scripting/Source Classes/EnemyDamageText.md>)
