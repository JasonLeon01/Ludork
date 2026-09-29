# GlobalCore Gameplay API

`require("GlobalCore")` exposes the native ability system, which covers `AttributeSet`, `AbilitySystemComponent`, and the ability, result, event, Effect and spec types.

The signatures below describe Lua instance calls. Blueprint nodes for these instance methods additionally expose a typed first `self` input, defaulting to the graph owner when unset. Connect the intended Attribute Set, Ability System Component or Ability instance when it differs from that owner.

## AttributeSet

An `AttributeSet` subclass declares ordered `ATTRIBUTE_NAMES` and `{ type, default }` entries in `SCHEMA`. `ID` identifies the General Data member. Missing fields use the schema defaults, and an explicit stored nil branch overrides a default. Stored General Data is loaded through `Source.Configs.GeneralDataTypes.Create`.

| Member | Signature | Behaviour |
|---|---|---|
| `init` | `(values?)` | Initialises every declared attribute and `ID` |
| `initialize` | `(values)` | Copies and validates runtime values |
| `initializeStored` | `(values)` | Materialises schema-directed stored values before copying them |
| `getAttributeNames` | `() -> string[]` | Returns a copy of the ordered names |
| `getAttributeSchema` | `(name) -> AttributeSchema?` | Returns the attribute's `type` schema and `default` value |

Only `int` and `float` fields have Base and Current values and modifiers. Values must be finite. `int` values require a Lua integer, while `float` values accept any finite number. Other fields remain direct typed data.

## AbilitySystemComponent

`AbilitySystemComponent.new(owner, attributeSet)` owns attributes, abilities, active Effects and counted tags.

### Attributes

| Member | Signature | Behaviour |
|---|---|---|
| `getOwner` | `() -> any` | Returns the owning gameplay object |
| `getAttributeSet` | `() -> AttributeSet` | Returns the live Attribute Set |
| `getNumericAttribute` | `(name) -> number` | Returns the current value after active modifiers |
| `getNumericAttributeBase` | `(name) -> number` | Returns the unmodified base value |
| `setNumericAttributeBase` | `(name, value)` | Changes one Base and recalculates every numeric Current |
| `setNumericAttributeBases` | `(values)` | Changes several base values, then recalculates all numeric attributes |
| `getNumericAttributeBases` | `() -> table<string, number>` | Returns a copy suitable for persistence |
| `addAttributeChangeListener` | `(name, callback, params?)` | Calls `callback(oldValue, newValue, change, ...)` after a relevant Current or Base change |
| `setNumericAttributeConstraint` | `(name, callback?)` | Installs or removes a final Current-value constraint and recalculates immediately |
| `getRevision` | `() -> integer` | Returns the revision changed by attribute, ability or effect mutations. Attribute listeners observe the revision after this commit |

Base setters validate the complete candidate Base and Current state before committing. Unknown attributes, non-finite values or schema mismatches fail without partial mutation. Direct numeric field assignment is treated as a Base write.

Listener `oldValue` and `newValue` are always Current values. The third argument is an `AttributeChange` context:

| Field | Type | Meaning |
|---|---|---|
| `source` | `"Base" \| "Effect" \| "Constraint"` | Operation that caused resolution |
| `force` | `boolean` | A Base write may notify even when visible Current did not change |
| `oldBase` | `number \| Class.MissingValue \| nil` | Previous Base for a `Base` source; otherwise nil |
| `newBase` | `number \| nil` | New Base for a `Base` source; otherwise nil |

Listener `params` follow `change`, and batch writes notify only the final state. Base writes notify even when Current is unchanged, while Effect and Constraint notifications require a Current change.

A constraint receives `(value, abilitySystem, resolvedValues)` after modifier aggregation. It must return exactly one schema-valid finite number, and it must never write Base. Passing nil removes it. Resolution follows `ATTRIBUTE_NAMES`, with HP last. `resolvedValues` contains the fields already resolved in that pass.

### Abilities and events

| Member | Signature | Behaviour |
|---|---|---|
| `giveAbility` | `(ability, sourceKey?) -> GameplayAbilitySpec` | Grants an ability and records its source |
| `removeAbilitiesBySource` | `(sourceKey)` | Removes every ability granted by that exact source |
| `tryActivateAbility` | `(abilityID, eventData?) -> GameplayAbilityResult` | Activates the highest-priority eligible ability with that ID |
| `handleGameplayEvent` | `(eventData) -> GameplayAbilityResult[]` | Runs every ability whose trigger tag matches the event |

Abilities run in descending `priority` order, then in grant order. Tags match hierarchically. Direct activation returns `AbilityNotFound` for a missing ID, or `AbilityNotActivated` when every matching ability fails its gate.

### Effects and tags

| Member | Signature | Behaviour |
|---|---|---|
| `applyGameplayEffectSpec` | `(spec) -> integer?` | Applies an Instant effect or returns an Infinite effect handle |
| `validateGameplayEffectSpec` | `(spec) -> bool` | Validates and previews the spec without changing ability system state |
| `removeActiveGameplayEffect` | `(handle, stacks?) -> bool` | Removes stacks or the whole active effect |
| `getActiveEffectStacks` | `(effectID) -> integer` | Sums stacks across active effects with that ID |
| `getActiveGameplayEffects` | `() -> ActiveGameplayEffect[]` | Returns active effects in application order |
| `hasMatchingGameplayTag` | `(tag) -> bool` | Tests an exact or child owned tag |

Instant modifiers update Base in order, apply each modifier's `minimum` immediately and return no handle. Infinite modifiers recalculate Current until they are removed. Add magnitudes sum, and Multiply magnitudes multiply. The latest Override wins, then the highest matching `minimum` clamps Current. Aggregate stacks scale Add linearly and Multiply exponentially. `None` deduplicates the same non-empty ID and source pair.

Magnitudes, minima, Base and Current must be finite and schema-valid. Instant Effects cannot grant tags or abilities. `None` requires one stack, and Aggregate Infinite Effects require a non-empty ID. Removing an Infinite Effect also removes its tags and granted abilities.

Applying, stacking and removing Effects preview the complete resulting ability system state before mutation. `validateGameplayEffectSpec` performs that preview without mutation, and application validates again. Magnitude functions and constraints run during preview and must avoid external side effects. Listener errors after commit are not rolled back.

## GameplayAbility

`GameplayAbility` defines activation policy and behaviour.

| Field | Type | Default |
|---|---|---|
| `id` | `string` | `""` |
| `priority` | `integer` | `0` |
| `abilityTags` | `string[]` | `{}` |
| `requiredTags` | `string[]` | `{}` |
| `blockedTags` | `string[]` | `{}` |
| `triggerTags` | `string[]` | `{}` |

| Member | Signature | Behaviour |
|---|---|---|
| `canActivate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | Checks required and blocked owned tags |
| `calculate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | Computes a result without committing by default |
| `activate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | Delegates to `calculate`; subclasses override it to commit work |

Granted abilities require a non-empty ID, and overrides must return `GameplayAbilityResult`.

## GameplayAbilityResult

`GameplayAbilityResult` contains `ok`, a string or integer `code` (default `""`), and arbitrary `data`.

| Static member | Signature | Behaviour |
|---|---|---|
| `Success` | `(code?, data?) -> GameplayAbilityResult` | Creates a successful result; default code is `Success` |
| `Failure` | `(code, data?) -> GameplayAbilityResult` | Creates a failed result and requires a non-empty string or non-zero integer code |

## GameplayEventData

`GameplayEventData.new(instigator?, target?, eventTag?, payload?)` carries one activation or event.

| Field | Type | Default |
|---|---|---|
| `instigator` | `any` | `nil` |
| `target` | `any` | `nil` |
| `eventTag` | `string` | `""` |
| `payload` | `table<string, any>` | `{}` |

Handlers may update `payload`. `handleGameplayEvent` requires a non-empty tag, while direct activation does not.

## GameplayEffect and GameplayEffectSpec

`GameplayEffect` describes modifiers and grants, and `GameplayEffectSpec` binds an Effect to event data, stacks and source identity.

| GameplayEffect field | Type | Values/default |
|---|---|---|
| `id` | `string` | `""` |
| `durationPolicy` | `string` | `Instant` or `Infinite`; default `Instant` |
| `stackingPolicy` | `string` | `None` or `Aggregate`; default `None` |
| `modifiers` | `GameplayModifier[]` | `{}` |
| `grantedTags` | `string[]` | `{}`; Infinite only |
| `grantedAbilities` | `GameplayAbility[]` | `{}`; Infinite only |
| `data` | `table<string, any>` | `{}` |

| GameplayModifier field | Type | Values/default |
|---|---|---|
| `attribute` | `string` | Target numeric attribute |
| `operation` | `string` | `Add`, `Multiply` or `Override` |
| `magnitude` | `number` or `function` | Required at validation: a finite number, or `(spec, stacks)` returning exactly one finite number |
| `minimum` | `number?` | Optional finite lower bound |

`data` is opaque to the ability system. `GameplayEffectSpec.new(effect, eventData?, stacks?, sourceKey?)` requires positive integer stacks. Effect ID plus Lua-equal `sourceKey` identifies an Infinite Effect for deduplication or stacking. Matching definitions are a caller invariant. Aggregate retains the first active spec.

## Runtime specs

`GameplayAbilitySpec` stores `ability`, `sourceKey` and `grantOrder`. `ActiveGameplayEffect` stores `handle`, `spec`, `stacks`, `applicationOrder` and granted ability specs.

## Scope

Supported: Instant and Infinite Effects, Add, Multiply and Override modifiers, aggregate stacking, hierarchical tags and synchronous abilities. Unsupported: timers, periodic Effects, prediction, replication, async tasks and built-in persistence.
