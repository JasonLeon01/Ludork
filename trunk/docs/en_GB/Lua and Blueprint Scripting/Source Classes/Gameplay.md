# Source Gameplay API

`Source.Gameplay` provides the Default Gameplay Scene contract, abilities, Effects, combat rules, movement-hazard helpers and generated General Data Attribute Sets built on `GlobalCore`.

## GameplayScene

**Module:** `Source.Gameplay.GameplayScene`  
**Inherits:** `GlobalCore.SceneBase`

`SceneMap` implements this contract, and Actors reach it through their owning map. Instance methods omit the implicit `self` receiver (`Source.Gameplay.GameplayScene`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getGameMap` | — | `Global.GameMap` | Returns the scene's loaded map. |
| `getGameInstance` | — | `Source.GameInstance` | Returns the game instance that owns variables and persistence state. |
| `recordAddedActor` | `actor: Engine.Actor` | — | Records an added Actor in persistence state. |
| `recordDestroyedActor` | `actor: Engine.Actor` | — | Records a destroyed Actor in persistence state. |
| `recordActorPosition` | `actor: Engine.Actor`, `position` | — | Records the Actor's map position. |
| `requestFloorStep` | `teleporter`, `step: int` | `bool` | Accepts one adjacent-floor request; `step` must be `1` or `-1`. Arrival selects the nearest visible, surviving stair of the opposite direction. |
| `requestMapTransfer` | `teleporter`, `mapPath: string`, `position: sf.Vector2i`, `record?: bool = true` | `bool` | Accepts one transfer to the specified tile without searching for a stair; optionally records each endpoint on its own map. |
| `requestGameOver` | `player`, `delay: number` | — | Waits for a finite non-negative animation delay and navigates without applying damage or Effects. |

Combat and movement hazards require a `GameplayScene` before they change state. Unavailable or rejected Teleporter requests preserve movement and records. Game Over uses the longest remaining animation duration. Duplicate requests are ignored, and replacing the scene, map or player cancels pending navigation.

## GeneralDataGraphAbility

**Module:** `Source.Gameplay.GeneralDataGraphAbility`  
**Inherits:** `GlobalCore.GameplayAbility`

| Field | Type | Default | Notes |
|---|---|---|---|
| `generalType` | `string` | — | General Data type key. |
| `memberID` | `string` | — | General Data member identifier. |
| `graphEvent` | `string` | — | Member graph event name. |

Instance methods omit the implicit `self` receiver (`Source.Gameplay.GeneralDataGraphAbility`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `init` | `generalType: string`, `memberID: string`, `graphEvent: string`, `triggerTags?` | — | Creates ability ID `GeneralData.<type>.<member>.<event>`. |
| `calculate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Returns `ExecutableGraph` with graph data or successful `NoGraph`. |
| `activate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Builds and synchronously executes the selected event; returns `GraphExecuted` or `NoGraph`. |

The graph parent and the `eventData` argument are the same `GameplayEventData`. Its payload receives the three fields above and, for State graphs, `activeEffect`. Execution uses `Engine.BPBase.ExecuteGraph`. General Data graphs have no Actor dispatch or inheritance, cannot be latent and propagate errors.

## Effects

**Module:** `Source.Gameplay.Effects`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `CreateInstantModifierSpec` | `effectID: string`, `attribute: string`, `operation`, `magnitude: number`, `eventData?: GlobalCore.GameplayEventData` | `GlobalCore.GameplayEffectSpec` | Creates, without applying, one Instant modifier spec. |
| `ApplyInstantModifier` | `target`, `effectID: string`, `attribute: string`, `operation`, `magnitude: number`, `eventData?: GlobalCore.GameplayEventData` | — | Applies one Instant modifier. |
| `CreateEquipmentEffect` | `equipID: string`, `slot: string`, `attrPlus` | `GlobalCore.GameplayEffect` | Creates an Infinite equipment effect and an `Equipment.Slot.<slot>` tag. |
| `CreateStateEffect` | `stateID: string` | `GlobalCore.GameplayEffect` | Creates an Infinite state effect, tag and granted state abilities. |
| `CreateStateSpec` | `stateID: string`, `stacks?: int`, `eventData?: GlobalCore.GameplayEventData` | `GlobalCore.GameplayEffectSpec` | Creates, without applying, a State spec with a positive stack count. |
| `ApplyState` | `target`, `stateID: string`, `stacks?: int`, `eventData?: GlobalCore.GameplayEventData` | `int` | Applies or stacks the State effect and returns its handle. |
| `RemoveState` | `target`, `stateID: string` | `bool` | Removes the complete State effect. |
| `ReduceState` | `target`, `stateID: string`, `stacks?: int` | `bool` | Removes only the requested stacks when supported. |
| `ClearStates` | `target` | — | Removes every effect whose ID begins with `State.`. |
| `GetStateStacks` | `target` | `table<string, int>` | Returns State ID to stack count. |
| `GetStateIDs` | `target` | `string[]` | Returns active State IDs in stable order. |
| `FindActiveEffectHandle` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `effectID: string` | `int?` | Returns the first matching active handle. |

Equipment uses `attrPlus` in stable attribute order. Weak applies `-1 ATK` and `-1 DEF` per stack, with a minimum of `0`. Poisoned grants its damage ability. States may grant `onWalk` and `onHookTriggered` graph abilities.

## MotaBattleAbility

**Module:** `Source.Gameplay.MotaBattleAbility`  
**Inherits:** `GlobalCore.GameplayAbility`  
**Ability ID:** `Ability.Combat.MotaBattle`

Load these constant modules directly with `require`; `MotaBattleAbility` returns their scalar values in `result.code`.

| Enum | Members |
|---|---|
| `Enums.BattleResultCode` | `WIN = 1`, `CANNOT_DAMAGE = 2`, `LETHAL_COUNTER_DAMAGE = 3` |
| `Enums.CriticalResultCode` | `VALUE = 1`, `NOT_NEEDED = 2`, `UNKNOWN = 3` |

Instance methods omit the implicit `self` receiver (`Source.Gameplay.MotaBattleAbility`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `calculate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Produces an `Enums.BattleResultCode` code and prevalidated damage/Game Over specs without mutation. |
| `activate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Calculates and optionally commits when `eventData.payload.commit` is `true`. |
| `CalculateDamagePerRound` | `attacker`, `defender` | `damage: number`, `details: table` | Static. Runs the attack, defence, damage, hit-count and incoming-damage event pipeline. |
| `CommitResult` | `result: GlobalCore.GameplayAbilityResult` | — | Static. Applies the prevalidated counter-damage Effect exactly once. |
| `CalculateCriticalValue` | `enemy`, `player` | `GlobalCore.GameplayAbilityResult` | Static. Returns an `Enums.CriticalResultCode` code. |

The owner is the enemy, and `eventData.target` is the player. Calculation validates without applying counter-damage or Game Over Specs. Its battle result data includes per-round damage, counter rounds, Vampire healing, first-strike and fixed damage, both Battlers, prepared Specs and `committed`. `CommitResult` applies the combined damage once, and the Enemy handles Game Over after the animation. Critical calculation includes Vampire healing and uses the same round-count threshold regardless of First, but ignores FixDmg expressions.

Combat resolution passes return strings from `Enums.CombatResolutionCode`: `ATTACK = "AttackResolved"`, `DEFENSE = "DefenseResolved"`, `HIT_COUNT = "HitCountResolved"`, `DAMAGE = "DamageResolved"`, `INCOMING_DAMAGE = "IncomingDamageResolved"` and `BATTLE_RULES = "BattleRulesResolved"`. These intermediate results are separate from the final `Enums.BattleResultCode` outcome.

## GameplayConstants

**Module:** `Source.Configs.GameplayConstants`

This module owns the shared Gameplay event tags and identifier prefixes used by event publishers and ability subscribers.

| Field | Value |
|---|---|
| `MOVEMENT_HAZARD_TAG` | `"Gameplay.Movement.Hazard"` |
| `BATTLE_RULES_EVENT` | `"Event.Combat.ResolveBattleRules"` |
| `MOVEMENT_STEP_EVENT` | `"Event.Movement.Step"` |
| `MOVEMENT_QUERY_HAZARD_EVENT` | `"Event.Movement.QueryHazard"` |
| `MOVEMENT_QUERY_HAZARD_BOUNDS_EVENT` | `"Event.Movement.QueryHazardBounds"` |
| `COMBAT_RESOLVE_ATTACK_EVENT` | `"Event.Combat.ResolveAttack"` |
| `COMBAT_RESOLVE_DEFENSE_EVENT` | `"Event.Combat.ResolveDefense"` |
| `COMBAT_RESOLVE_HIT_COUNT_EVENT` | `"Event.Combat.ResolveHitCount"` |
| `COMBAT_RESOLVE_DAMAGE_EVENT` | `"Event.Combat.ResolveDamage"` |
| `COMBAT_RESOLVE_INCOMING_DAMAGE_EVENT` | `"Event.Combat.ResolveIncomingDamage"` |
| `EQUIPMENT_EQUIP_EVENT` | `"Event.Equipment.Equip"` |
| `SPECIAL_PREFIX` | `"Special."` |
| `STATE_PREFIX` | `"State."` |
| `EQUIPMENT_PREFIX` | `"Equipment."` |

## SpecialAbilities

**Module:** `Source.Gameplay.SpecialAbilities`

Concrete ability classes can also be loaded from `Source.Gameplay.SpecialAbilities.<ClassName>`, for example `require("Source.Gameplay.SpecialAbilities.MultiHitAbility").new(magnitude)`.

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `CreateEffect` | `specialID: string`, `magnitude: any` | `GlobalCore.GameplayEffect` | Creates one Infinite `Special.<id>` effect, tag and ability. |
| `CreatePoisonedAbility` | — | `GlobalCore.GameplayAbility` | Creates the State Poisoned incoming-damage ability. |
| `GetMagnitude` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `specialID: string` | `any` | Reads the magnitude stored on the active special effect. |

Compete, Hard, Magic, MultiHit and Poisoned use their combat resolution events. `GameplayConstants.BATTLE_RULES_EVENT` gathers Vampire healing, First damage and FixDmg. Numeric FixDmg applies directly, while string FixDmg substitutes the player's and enemy's current numeric attributes before evaluation. Domain, Flank and Blockade handle `Event.Movement.QueryHazard` and grant `Gameplay.Movement.Hazard`. Other IDs grant queryable passive tags.

## MovementSpecials and movement danger

**Module:** `Source.Utils.MovementSpecials`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `CreatePreviewContext` | `enemies`, `player` | `PreviewContext` | Validates hazard coverage and caches combat damage and coverage radii for one unchanged player/enemy snapshot. |
| `Preview` | `enemies`, `player`, `playerPosition: sf.Vector2i`, `ignoredEnemies?`, `previewContext?` | `GlobalCore.GameplayAbilityResult` | Resolves movement-hazard damage and contributing sources. |
| `Commit` | `player`, `pathPositions: sf.Vector2i[]` | `GlobalCore.GameplayAbilityResult` | Reuses `Preview` for every traversed cell, applies total damage and performs Blockade retreat. |
| `NotifyPlayerMovementFinished` | `player`, `pathPositions?: sf.Vector2i[]` | `GlobalCore.GameplayAbilityResult` | Commits a completed movement path or the current cell. |

Load `local MovementResultCode = require("Enums.MovementResultCode")` for movement results: `HAZARD_BOUNDS = "MovementHazardBounds"`, `HAZARD = "MovementHazard"`, `NO_HAZARD = "NoMovementHazard"` and `HAZARD_COMMITTED = "MovementHazardCommitted"`.

`Preview` owns movement-hazard geometry. It dispatches `Event.Movement.QueryHazard`, resolves Flank and ignored enemies, and returns ordered sources and total damage. Domain activates when Manhattan distance is `<= max(1, magnitude)`, including the range boundary, and uses `MotaBattleAbility.CalculateDamagePerRound` with combat specials such as Magic. Ignored Flank enemies are included in geometry but not in damage.

Pass movement-hazard enemies, such as those returned by `CollectEnemies`, to `CreatePreviewContext`. Every granted ability subscribed to `Event.Movement.QueryHazard` must also handle `Event.Movement.QueryHazardBounds`. The bounds event uses the enemy as `instigator`, the player as `target` and an empty payload.

Each bounds handler must successfully return `GameplayAbilityResult.Success(MovementResultCode.HAZARD_BOUNDS, { radius = n })`, where `n` is a non-negative integer. The inclusive square from `(enemy.x - n, enemy.y - n)` to `(enemy.x + n, enemy.y + n)` must contain every cell where that ability can contribute damage. Domain declares `max(1, magnitude)`; Blockade and Flank declare `1`. Missing subscriptions, rejected queries and malformed results raise errors, including when another ability on the same enemy supplies valid bounds. Bounds and damage previews must not mutate gameplay state.

`PreviewContext` contains `player`, `damageByEnemy` and `radiusByEnemy`; the radius is the largest declared by that enemy's abilities. Recreate it when enemy membership, visibility, positions or player/enemy ability state changes. Danger grids filter by `Gameplay.Movement.Hazard` and materialise on demand. They clip coverage to the requested area, run `Preview` only for candidate cells and relevant enemies, and preserve source and row-major entry order. Coverage is conservative: per-cell events and Flank resolution determine the exact damage. Unchanged overlapping cells, including zero-damage cells, are reused when the area changes.

## Generated GeneralDataTypes

**Module:** `Source.Configs.GeneralDataTypes`

The General Data generator owns this module and its stub. `Types` and named fields expose each generated `<TypeName>AttributeSet`. Type keys come from `Enums.GeneralDataKey`; member keys come from the matching `Enums.GeneralData.<TypeName>` module.

`Create(typeName, memberID, memberData)` returns `GlobalCore.AttributeSet`, deep-copying schema fields and setting `ID`. Use `Source.Data.CreateGeneralAttributeSet(typeName, memberID)` when starting from IDs.
