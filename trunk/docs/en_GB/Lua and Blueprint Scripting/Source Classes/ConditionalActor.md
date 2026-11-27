# Source.MapActors.ConditionalActor

`ConditionalActor` is a map Actor base class that controls visibility from a game-variable condition. Configure its variable name, operator and comparison value to show or hide an Actor dynamically at runtime.

**Metadata source:** `Scripts/Source/MapActors/ConditionalActor_meta.lua`

**Inherits:** `Engine.Actor`

## Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `conditionVariable` | `string` | `""` | `InstVar` selector restricted to `int`, `float`, `bool` and `string`. |
| `conditionOperator` | `string` | `"=="` | Drop-down: `==`, `~=`, `>`, `>=`, `<`, `<=`. |
| `conditionValue` | `any` | `0` | `InstVarValue = "conditionVariable"`; runtime accepts `number`, `bool` or `string`. |

An empty variable name leaves the authored `visible` value unchanged. Otherwise, the current variable and comparison value must have matching scalar types. Numbers support all six operators; booleans and strings support only equality and inequality. Missing variables, unsupported operators and mismatched or unsupported types raise errors without implicit conversion.

The condition continuously controls `setVisible(result, false)`: a `false` result hides the Actor again. Child Actors keep their own visibility settings and follow their ancestors' effective visibility. Hidden Actors keep their instances and subscriptions, and Tick stays governed by `tickable` and world streaming.

`applyConditionVisibility(variables)` evaluates the same condition against the supplied game-variable table and applies visibility once. It requires no owning Scene, registers no subscription and dispatches no lifecycle events. An empty condition preserves the Actor's current visibility.

Live Actors and floor previews share the scalar evaluator `Source.Utils.ActorVisibility.Evaluate(variable, operator, comparison, authoredVisible, variables)`. Previews apply its result to static display records after restoring map records, without creating Actors or subscriptions. A hidden record stays cached for later re-evaluation, and a child's effective visibility also requires its ancestors to be visible.

## Lifecycle

`onCreate()` subscribes to `GameInstance:getVariables()` and immediately evaluates the condition. A subscription uses the current map path and a non-empty `getMapTag()`. Multiple Actors may watch the same field independently. A failed initial evaluation releases the subscription before propagating the error.

`onDestroy()` and `onWorldSleep()` unsubscribe. `onWorldWake(elapsedSeconds)` subscribes again and re-evaluates current variables. Overrides in Lua or Script Mixins must call `super()`. Blueprint lifecycle graphs must continue the parent event; the Game project's empty lifecycle graphs inherit this automatically.

`releaseConditionMonitor()` performs repeatable subscription cleanup without changing visibility or persistent destruction records. `ConditionalActor.ReleaseMapMonitors(gameMap)` releases all condition subscriptions belonging to a departing map. `SceneMap` uses it before map replacement and destruction, and releases a suppressed world Actor tree before removing its identity. Temporarily covering a Scene preserves its subscriptions for re-entry.

No subscription or derived visibility state is saved. Variables keep their current logical values in the save snapshot. Map restoration removes destroyed placements before `onCreate`, so collected pickups do not reappear.

## Inheritance

`Enemy`, `Item`, `Equip`, `DoorBase` and `Teleporter` inherit this class. `Bottle`, `Gem` and the Game project's other direct map-Actor Blueprints use `Source.MapActors.ConditionalActor.ConditionalActor` as their parent. `EnemyDamageText` inherits `Engine.Actor` and uses its own damage-hint logic.
