# GlobalCore Gameplay API

`require("GlobalCore")` exposes the native ability system covering `AttributeSet`, `AbilitySystemComponent`, abilities, results, events, Effects and runtime specs. Instance method tables below omit the implicit Lua `self` receiver; Blueprint nodes expose it as a typed first `self` input defaulting to the graph owner when unset (connect the intended `AttributeSet`, `AbilitySystemComponent` or `GameplayAbility` when it differs from the owner).

## AttributeSet

An `AttributeSet` subclass declares ordered `ATTRIBUTE_NAMES` and `{ type, default }` entries in `SCHEMA`, with `ID` identifying the General Data member. Missing fields use schema defaults, while an explicit stored `nil` branch overrides a default. Stored General Data loads through `Source.Configs.GeneralDataTypes.Create`.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `init` | `values?: table` | — | Initialises every declared attribute and `ID`. |
| `initialize` | `values: table` | — | Copies and validates runtime values. |
| `initializeStored` | `values: table` | — | Materialises schema-directed stored values before copying them. |
| `getAttributeNames` | — | `string[]` | Returns a copy of the ordered attribute names. |
| `getAttributeSchema` | `name: string` | `AttributeSchema?` | Returns the attribute's `type` schema and `default` value. |

Only `int` and `float` fields maintain Base and Current values and accept modifiers. All numeric values must be finite: `int` requires a Lua integer, while `float` accepts any finite number. Other fields store direct typed data.

## AbilitySystemComponent

`AbilitySystemComponent.new(owner, attributeSet)` creates a component owning attributes, granted abilities, active Effects and counted gameplay tags.

### Attributes

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getOwner` | — | `any` | Returns the owning gameplay object. |
| `getAttributeSet` | — | `GlobalCore.AttributeSet` | Returns the live Attribute Set. |
| `getNumericAttribute` | `name: string` | `number` | Returns the Current value after active modifiers. |
| `getNumericAttributeBase` | `name: string` | `number` | Returns the unmodified Base value. |
| `setNumericAttributeBase` | `name: string`, `value: number` | — | Updates one Base value and recalculates every numeric Current value. |
| `setNumericAttributeBases` | `values: table<string, number>` | — | Updates multiple Base values, then recalculates all numeric attributes. |
| `getNumericAttributeBases` | — | `table<string, number>` | Returns a copy suitable for persistence. |
| `addAttributeChangeListener` | `name: string`, `callback: function`, `params?: any[]` | — | Calls `callback(oldValue, newValue, change, ...)` after a relevant Current or Base change. |
| `setNumericAttributeConstraint` | `name: string`, `callback?: function` | — | Installs or removes (`nil`) a final Current-value constraint and recalculates immediately. |
| `getRevision` | — | `integer` | Returns the component revision, incremented on attribute, ability or effect mutations. Attribute listeners observe the post-commit revision. |

Base setters validate the complete candidate Base and Current state before committing. Unknown attributes, non-finite values or schema mismatches fail without partial mutation. Direct numeric field assignment is treated as a Base write.

Listener `oldValue` and `newValue` arguments are always Current values, followed by an `AttributeChange` context and any bound `params`:

| Field | Type | Default | Notes |
|---|---|---|---|
| `source` | `"Base" \| "Effect" \| "Constraint"` | — | Operation that triggered resolution: `GameplayAttributeChangeSource.BASE` (`"Base"`), `EFFECT` (`"Effect"`) or `CONSTRAINT` (`"Constraint"`). |
| `force` | `boolean` | — | `true` when a Base write notifies even if the visible Current value did not change. |
| `oldBase` | `number \| Class.MissingValue \| nil` | — | Previous Base for a `"Base"` source; otherwise `nil`. |
| `newBase` | `number \| nil` | — | New Base for a `"Base"` source; otherwise `nil`. |

The source constants are static, read-only fields on `GlobalCore.GameplayAttributeChangeSource`; `change.source` is always a string.

- **Notifications:** Batch writes notify only the final state. Base writes notify even when Current is unchanged, while Effect and Constraint notifications fire only when Current changes.
- **Constraints:** A constraint receives `(value, abilitySystem, resolvedValues)` after modifier aggregation, must return exactly one schema-valid finite number and must never write Base. Passing `nil` removes it. Resolution runs in `ATTRIBUTE_NAMES` order with HP last; `resolvedValues` holds fields already resolved in the current pass.

### Abilities and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `giveAbility` | `ability: GlobalCore.GameplayAbility`, `sourceKey?: any` | `GlobalCore.GameplayAbilitySpec` | Grants an ability and records its source. |
| `removeAbilitiesBySource` | `sourceKey: any` | — | Removes every ability granted by that exact source. |
| `tryActivateAbility` | `abilityID: string`, `eventData?: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Activates the highest-priority eligible ability with that ID. |
| `getGameplayEventAbilities` | `eventTag: string` | `GlobalCore.GameplayAbilitySpec[]` | Returns subscribed granted specs in event execution order without calling gates or abilities. |
| `handleGameplayEvent` | `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult[]` | Runs every ability whose trigger tag matches the event. |

Abilities execute in descending `priority` order, then in grant order, using hierarchical tag matching. Direct activation returns `AbilityNotFound` for a missing ID or `AbilityNotActivated` when every matching ability fails its gate.

`getGameplayEventAbilities` uses the same tag matching and ordering as `handleGameplayEvent`, covering abilities granted directly or by Effects. It requires a non-empty event tag and returns an empty array when none match. Mutating the returned array does not grant or remove abilities, and calling it does not invoke `canActivate` or `activate` or change the component revision.

### Effects and tags

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `applyGameplayEffectSpec` | `spec: GlobalCore.GameplayEffectSpec` | `integer?` | Applies an Instant Effect (returns `nil`) or returns an Infinite Effect handle. |
| `validateGameplayEffectSpec` | `spec: GlobalCore.GameplayEffectSpec` | `bool` | Validates and previews the spec without changing ability system state. |
| `removeActiveGameplayEffect` | `handle: integer`, `stacks?: integer` | `bool` | Removes the requested stacks or the entire active Effect. |
| `getActiveEffectStacks` | `effectID: string` | `integer` | Sums stacks across active Effects with that ID. |
| `getActiveGameplayEffects` | — | `GlobalCore.ActiveGameplayEffect[]` | Returns active Effects in application order. |
| `hasMatchingGameplayTag` | `tag: string` | `bool` | Tests for an exact or child owned tag. |

Effect application and modifier resolution follow these rules:

- **Instant vs Infinite:** Instant modifiers update Base in order, apply each modifier's `minimum` immediately and return no handle. Infinite modifiers recalculate Current until removed, and removing an Infinite Effect also removes its granted tags and abilities.
- **Aggregation and stacking:** `Add` magnitudes sum and `Multiply` magnitudes multiply; the latest `Override` wins, and the highest matching `minimum` clamps Current. `Aggregate` stacks scale `Add` linearly and `Multiply` exponentially. `None` deduplicates by non-empty ID and source pair.
- **Validation constraints:** Magnitudes, minima, Base and Current must be finite and schema-valid. Instant Effects cannot grant tags or abilities. `None` requires one stack, and `Aggregate` Infinite Effects require a non-empty ID.

> **Important:** Applying, stacking and removing Effects preview the complete resulting state before committing, and `validateGameplayEffectSpec` runs that preview without mutating state. Magnitude functions and constraints execute during preview and must avoid external side effects. Listener errors raised after commit are not rolled back.

## GameplayAbility

`GameplayAbility` defines activation policy, tags and execution hooks for an ability.

| Field | Type | Default | Notes |
|---|---|---|---|
| `id` | `string` | `""` | Granted abilities require a non-empty ID. |
| `priority` | `integer` | `0` | Higher priorities run first. |
| `abilityTags` | `string[]` | `{}` | — |
| `requiredTags` | `string[]` | `{}` | — |
| `blockedTags` | `string[]` | `{}` | — |
| `triggerTags` | `string[]` | `{}` | — |

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `canActivate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Checks required and blocked owned tags. |
| `calculate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Computes a result without committing by default. |
| `activate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | Delegates to `calculate`; override in subclasses to commit work. |

All method overrides must return a `GameplayAbilityResult`.

## GameplayAbilityResult

`GameplayAbilityResult` carries a boolean `ok`, a string or integer `code` (default `""`) and arbitrary `data`.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `Success` | `code?: string \| integer`, `data?: any` | `GlobalCore.GameplayAbilityResult` | Static. Creates a successful result; defaults `code` to `"Success"`. |
| `Failure` | `code: string \| integer`, `data?: any` | `GlobalCore.GameplayAbilityResult` | Static. Creates a failed result; requires a non-empty string or non-zero integer `code`. |

Built-in result codes are static, read-only fields on `GlobalCore.GameplayAbilityResult`. Custom abilities may return their own string or integer codes.

| Constant | String value | Meaning |
|---|---|---|
| `CODE_SUCCESS` | `"Success"` | Default successful result. |
| `CODE_ABILITY_NOT_FOUND` | `"AbilityNotFound"` | No granted ability has the requested ID. |
| `CODE_ABILITY_NOT_ACTIVATED` | `"AbilityNotActivated"` | No matching ability passed its activation gate. |
| `CODE_MISSING_REQUIRED_TAG` | `"MissingRequiredTag"` | A required tag is absent. |
| `CODE_BLOCKED_BY_TAG` | `"BlockedByTag"` | A blocked tag is present. |

## GameplayEventData

`GameplayEventData.new(instigator?, target?, eventTag?, payload?)` carries context for one activation or event dispatch.

| Field | Type | Default | Notes |
|---|---|---|---|
| `instigator` | `any` | `nil` | — |
| `target` | `any` | `nil` | — |
| `eventTag` | `string` | `""` | Required to be non-empty for `handleGameplayEvent`; optional for direct activation. |
| `payload` | `table<string, any>` | `{}` | Handlers may mutate `payload`. |

## GameplayEffect and GameplayEffectSpec

`GameplayEffect` defines attribute modifiers and grants, and `GameplayEffectSpec` binds an Effect to event data, stacks and source identity.

**`GameplayEffect` fields:**

| Field | Type | Default | Notes |
|---|---|---|---|
| `id` | `string` | `""` | Required non-empty for `Aggregate` Infinite Effects. |
| `durationPolicy` | `string` | `GameplayEffect.DURATION_INSTANT` | `GameplayEffect.DURATION_INSTANT` (`"Instant"`) or `DURATION_INFINITE` (`"Infinite"`). |
| `stackingPolicy` | `string` | `GameplayEffect.STACKING_NONE` | `GameplayEffect.STACKING_NONE` (`"None"`) or `STACKING_AGGREGATE` (`"Aggregate"`). |
| `modifiers` | `GlobalCore.GameplayModifier[]` | `{}` | — |
| `grantedTags` | `string[]` | `{}` | Infinite Effects only. |
| `grantedAbilities` | `GlobalCore.GameplayAbility[]` | `{}` | Infinite Effects only. |
| `data` | `table<string, any>` | `{}` | Opaque to the ability system. |

**`GameplayModifier` fields:**

| Field | Type | Default | Notes |
|---|---|---|---|
| `attribute` | `string` | — | Target numeric attribute name. |
| `operation` | `string` | — | `GameplayModifier.OPERATION_ADD` (`"Add"`), `OPERATION_MULTIPLY` (`"Multiply"`) or `OPERATION_OVERRIDE` (`"Override"`). |
| `magnitude` | `number \| function` | — | Required at validation: a finite number, or `fun(spec, stacks)` returning exactly one finite number. |
| `minimum` | `number?` | `nil` | Optional finite lower bound. |

Policy and operation constants are static, read-only properties on their owning `GlobalCore` classes and hold the string values shown above.

`GameplayEffectSpec.new(effect, eventData?, stacks?, sourceKey?)` requires a positive integer `stacks` count. Effect ID combined with a Lua-equal `sourceKey` identifies an Infinite Effect for deduplication or stacking; matching definitions are a caller invariant, and `Aggregate` keeps the first active spec.

## Runtime specs

- `GameplayAbilitySpec` stores `ability`, `sourceKey` and `grantOrder`.
- `ActiveGameplayEffect` stores `handle`, `spec`, `stacks`, `applicationOrder` and granted ability specs.

## Scope

- **Supported:** Instant and Infinite Effects, `Add`, `Multiply` and `Override` modifiers, aggregate stacking, hierarchical tags and synchronous abilities.
- **Unsupported:** timers, periodic Effects, prediction, replication, async tasks and built-in persistence.
