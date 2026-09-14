# GlobalCore: Time Management

## TimerEntry

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `time` | `float` | — | — |
| `params` | `any[]` | — | — |
| `blocking` | `bool` | — | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `task` | `function` | — | — | — |
| `isReady` | `function` | self: { "GlobalCore", "TimerEntry" } = "self" | return: bool | — |
| `isCancelled` | `function` | self: { "GlobalCore", "TimerEntry" } = "self" | return: bool | Pure |
| `cancel` | `function` | self: { "GlobalCore", "TimerEntry" } = "self" | — | — |

## TimeManager

Direct metadata bases: —

The runtime publishes the current time, the delta time and the speed atomically. Once `TimeManager` is initialised, the getters for these values do not acquire the `TimeManager` writer lock. `getDeltaTime` still applies the currently published speed.

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `getCurrentTime` | `function` | — | return: sf.Time | — |
| `getDeltaTime` | `function` | — | return: sf.Time | — |
| `getSpeed` | `function` | — | return: float | — |
| `setSpeed` | `function` | speed: float | — | — |
