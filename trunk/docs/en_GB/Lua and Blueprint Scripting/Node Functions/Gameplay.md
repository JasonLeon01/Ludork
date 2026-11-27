# GlobalFunctions.Gameplay

`GlobalFunctions.Gameplay` provides Blueprint nodes for reading a General Data gameplay event context and modifying Battler attributes, States, Effects and gameplay events.

**Metadata source:** `Scripts/GlobalFunctions/Gameplay_meta.lua`

## Gameplay

### Functions

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `GetContext` | — | `context: GlobalCore.GameplayEventData` | Pure. Returns the current gameplay event context. |
| `GetSource` | — | `source: any` | Pure. Returns the event context's instigator/source. |
| `GetTarget` | — | `target: any` | Pure. Returns the event context's target. |
| `GetEventTag` | — | `eventTag: string` | Pure. Returns the active gameplay event tag. |
| `GetPayload` | — | `payload: any` | Pure. Returns the event context's payload table. |
| `HasTag` | `target: Source.Battler`, `tag: string` | `value: bool` | Pure. Checks whether `target` has the specified gameplay tag. |
| `GetNumericAttribute` | `target: Source.Battler`, `attribute: string` | `value: any` | Pure. Dynamic numeric pin. Reads a numeric attribute from `target`. |
| `SetNumericAttributeBase` | `target: Source.Battler`, `attribute: string`, `value: any` | — | Dynamic numeric pin. Writes the attribute's Base value on `target`. |
| `ApplyAttributeDelta` | `target: Source.Battler`, `attribute: string`, `magnitude: any` | — | Dynamic numeric pin. Applies an Instant Add modifier to `target`. |
| `ApplyState` | `target: Source.Battler`, `stateID: string`, `stacks: int = 1` | `handle: int` | `Enums.GeneralData.State` string enum selector. Applies or stacks a State on `target`. |
| `RemoveState` | `target: Source.Battler`, `stateID: string` | — | Execution branches: `Success`, `Failed`. `Enums.GeneralData.State` string enum selector. |
| `ReduceState` | `target: Source.Battler`, `stateID: string`, `stacks: int = 1` | — | Execution branches: `Success`, `Failed`. `Enums.GeneralData.State` string enum selector. |
| `RemovePlayerState` | `stateID: string` | — | `Enums.GeneralData.State` string enum selector. Removes the State from the context's target. |
| `ReducePlayerState` | `stateID: string`, `stacks: int = 1` | — | `Enums.GeneralData.State` string enum selector. Reduces the State stacks on the context's target. |
| `SendEvent` | `target: Source.Battler`, `eventTag: string`, `payload: any = {}` | `results: any` | Sends a synchronous gameplay event to `target`. |
| `ApplyEffect` | `target: Source.Battler`, `effect: any`, `stacks: int = 1`, `sourceKey: any` | `handle: int` | Applies a `GlobalCore.GameplayEffect` to `target`. |

The context accessors and the context-bound mutation nodes require a General Data graph owned by `GameplayEventData`. Using them in an ordinary Actor graph is an error. `RemovePlayerState` and `ReducePlayerState` act on that context's target and use a single execution output when no State exists.

Dynamic attribute pins are typed `any`, but the Ability System requires a finite number of the selected schema type. `SetNumericAttributeBase` writes Base; `ApplyAttributeDelta` applies an Instant Add modifier.

`SendEvent` runs synchronously, preserves the instigator and targets the supplied Battler. `ApplyEffect` accepts a `GameplayEffect`; Instant Effects return no persistent handle and cannot grant tags or abilities.
