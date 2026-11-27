# Source.MapActors.Enemy

`Enemy` is a conditionally visible Battler Actor bound to an Enemy General Data entry. It declares a damage-text child component, animation flags and post-battle variable changes, and exposes the `onCollision` and `onDefeat` events.

**Metadata source:** `Scripts/Source/MapActors/Enemy_meta.lua`

**Inherits:** `Source.MapActors.ConditionalActor`, `Source.Battler` (see [Source.MapActors.ConditionalActor](<ConditionalActor.md>) for inherited visibility conditions).

The Blueprint editor synthesises a read-only `attributes` preview of `Source.Configs.GeneralDataTypes.EnemyAttributeSet`. The preview is not serialised into the Blueprint JSON.

## Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | `Enums.GeneralData.Enemy` enum selector. |
| `childActorComp` | `Source.Components.ChildActorComponent` | Damage-text child | Component. |
| `collisionEnabled` | `bool` | `true` | — |
| `animatable` | `bool` | `true` | — |
| `animateWithoutMoving` | `bool` | `true` | — |
| `afterBattleVarChanges` | `Dict[string, Tuple[string, any]]` | `{}` | Numeric game-variable selector and fixed operator selector. |

The shared runtime setting `DefeatShatterEffectEnabled` is controlled by `enemyDefeatShatterEffectEnabled` in [Gameplay configuration](<../Default Gameplay/Runtime Data Configuration and Saves.md#shared-gameplay-configuration>). The `attributes` field holds the generated Enemy Attribute Set.

`Enemy` inherits the `Engine.Actor` default `tickable = false`.

## Functions and events

Events omit the implicit `self` receiver (`Source.MapActors.Enemy`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `onCollision` | `other: Engine.Actor[]` | — | Event. Default execution output. |
| `onDefeat` | — | — | Event. Default execution output. |

`init(texture?, rect?, tag?)` constructs the Attribute Set, the Ability System, the Mota ability and the configured special Effects. Collision prevalidates combat and required settlement before committing counter damage. On a loss, the prepared Game Over Effect is applied after the animations.

After a win animation, the Actor Blueprint's `onDefeat` hook runs before mandatory destruction, Reborn/drops, rewards and States. These values come from the prepared snapshot, and `afterBattleVarChanges` is evaluated after the hook. Enemy General Data has no `onDefeat` member graph.
