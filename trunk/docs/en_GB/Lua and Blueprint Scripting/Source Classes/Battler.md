# Source.Battler

`Battler` is the Lua gameplay base class of `Enemy` and `Player`. Each concrete subclass constructs a generated Attribute Set from General Data and owns an Ability System Component for attributes, abilities, effects and tags.

**Metadata source:** `Scripts/Source/Battler_meta.lua`

## Runtime state

| Field | Type | Default | Notes |
|---|---|---|---|
| `attributes` | `GlobalCore.AttributeSet` | — | Live generated Attribute Set; concrete subclasses narrow this to Enemy or Player. |
| `_abilitySystemComponent` | `GlobalCore.AbilitySystemComponent` | — | Private owner of attributes, abilities, effects and tags. |

## Runtime API

Instance methods omit the implicit `self` receiver (`Source.Battler`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `init` | `attributes: GlobalCore.AttributeSet` | — | Requires an Attribute Set and creates its Ability System Component. |
| `getAbilitySystemComponent` | — | `GlobalCore.AbilitySystemComponent` | Returns the live Ability System Component. |
| `getAttr` | `name: string`, `base?: bool = false` | `number` | Returns numeric Current, or Base when `base` is `true`. |
| `setAttr` | `name: string`, `value: number` | — | Sets numeric Base through the Ability System and recalculates Current. |
| `addAttr` | `name: string`, `delta: number` | — | Adds `delta` to numeric Base; negative values subtract. |
| `playAttackAnimationAt` | `scene`, `targetPosition` | `GlobalCore.Animation?` | Plays `attributes.ANIMATION_KEY` at the target and returns the animation, or `nil` when the key is empty. Callers can read its visual duration and time tags. |

Use `getAttr`, `setAttr` and `addAttr` for individual numeric attributes. `addAttr` reads Base, so active modifiers stay separate. These methods enforce native validation: unknown attributes, non-finite values and generated `int` or `float` schema mismatches fail. Read other fields directly from `attributes`, and use the Ability System for batch writes, persistence, Effects and listeners.
