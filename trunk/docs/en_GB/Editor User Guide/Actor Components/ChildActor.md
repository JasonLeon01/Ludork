# ChildActor Component

Spawn a child Actor from a class reference and position it relative to its owning Actor. Default Gameplay uses this component to display the enemy damage readout.

`Source.MapActors.Enemy` declares `childActorComp` (`Source.Components.ChildActorComponent`), making it available to Enemy Blueprints and their descendants; native `Engine.Actor` does not declare this field.

## Configuration

1. Open an Enemy Blueprint or select an Enemy map instance, then open `childActorComp` under **Components** (see [Actor Components](<Overview.md#configuration>)).
2. Set `className` to the full Actor class reference (Enemy defaults to `Source.MapActors.EnemyDamageText`; Blueprint children use paths such as `Data.Blueprints.Actors.Guard`). Leaving `className` empty spawns no child.
3. Set `relativePosition` to the child's local offset from the owner (default `[0, 0]`).
4. Save the Blueprint or map.

**API references:** [ChildActorComponent](<../../Lua and Blueprint Scripting/Source Classes/ChildActorComponent.md>) · [Enemy](<../../Lua and Blueprint Scripting/Source Classes/Enemy.md>) · [EnemyDamageText](<../../Lua and Blueprint Scripting/Source Classes/EnemyDamageText.md>)
