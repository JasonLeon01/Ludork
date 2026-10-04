# Source.MapActors.Enemy

`Enemy` declares the General Data identifier, damage-text child component, animation flags and post-battle variable changes of a battle-capable map Actor, and exposes the `onCollision` and `onDefeat` events.

Metadata source: `Scripts/Source/MapActors/Enemy_meta.lua`

Direct metadata bases: `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`, `{ "Source.Battler", "Battler" }`

Inherited visibility conditions: [Source.MapActors.ConditionalActor](<ConditionalActor.md>).

Meta: `{ GeneralDataVars = { { "ID", "Enemy" } } }`

The Blueprint editor synthesises a read-only `attributes` preview of `Source.Configs.GeneralDataTypes.EnemyAttributeSet`. The preview is not serialised into the Blueprint JSON.

## Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | GeneralDataVars = `Enemy` |
| `childActorComp` | `Source.Components.ChildActorComponent` | damage-text child | component |
| `collisionEnabled` | `bool` | `true` | — |
| `animatable` | `bool` | `true` | — |
| `animateWithoutMoving` | `bool` | `true` | — |
| `afterBattleVarChanges` | `Dict[string, Tuple[string, any]]` | `{}` | numeric game-variable selector and fixed operator selector |

The runtime class setting `DefeatShatterEffectEnabled` defaults to `true`. The `attributes` field holds the generated Enemy Attribute Set.

Enemy inherits the Actor default `tickable = false`.

## Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `onCollision` | event | `other: Engine.Actor[]` | — | default execution output |
| `onDefeat` | event | — | — | default execution output |

`init(texture?, rect?, tag?)` constructs the Attribute Set, the Ability System, the Mota ability and the configured special Effects. Collision prevalidates combat and required settlement before committing counter damage. A loss applies the prepared Game Over Effect after the animations.

After a win animation, the Actor Blueprint `onDefeat` hook runs before mandatory destruction, Reborn/drops, rewards and States. These values use the prepared snapshot, and `afterBattleVarChanges` is evaluated after the hook. Enemy General Data has no `onDefeat` member graph.

## Related pages

- [Source Gameplay API](<Gameplay.md>)
- [Enemy](<../../Editor User Guide/General Data Reference/Enemy.md>)
- [ChildActor component configuration](<../../Editor User Guide/Actor Components/ChildActor.md>)
