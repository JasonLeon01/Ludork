# Engine: Scalar Curves

## AnimSprite

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `setData` | `function` | self: { "Engine", "AnimSprite" } = "self"; animationData: { "Engine", "AnimationData" } | — | — |
| `getDuration` | `function` | self: { "Engine", "AnimSprite" } = "self" | return: float | Pure |
| `getVisualDuration` | `function` | self: { "Engine", "AnimSprite" } = "self" | return: float | Pure |
| `getAllTimeTags` | `function` | self: { "Engine", "AnimSprite" } = "self" | return: { "Engine", "AnimationTimeTag[]" } | Pure |
| `isFinished` | `function` | self: { "Engine", "AnimSprite" } = "self" | return: bool | Pure |
| `getFrameIndex` | `function` | self: { "Engine", "AnimSprite" } = "self" | return: int | Pure |
| `update` | `function` | self: { "Engine", "AnimSprite" } = "self"; deltaTime: float | — | — |
| `applyFrame` | `function` | self: { "Engine", "AnimSprite" } = "self"; frameIndex: int | — | — |

## CurveKey

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `value` | `float` | `0` | — |
| `interpolation` | `string` | `"linear"` | — |
| `arriveTangent` | `float` | `0` | — |
| `leaveTangent` | `float` | `0` | — |

### Functions and events

No Blueprint functions or events are declared.

## CurveData

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `type` | `string` | `"curve"` | — |
| `name` | `string` | — | — |
| `defaultValue` | `float` | `0` | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `{ "Engine", "CurveKey[]" }` | — | — |

### Functions and events

No Blueprint functions or events are declared.

## Curve

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `name` | `string` | — | — |
| `defaultValue` | `float` | `0` | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `{ "Engine", "CurveKey[]" }` | — | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `fromData` | `function` | data: { "Engine", "CurveData" } | return: { "Engine", "Curve" } | Pure |
| `toData` | `function` | self: { "Engine", "Curve" } = "self" | return: { "Engine", "CurveData" } | Pure |
| `isEmpty` | `function` | self: { "Engine", "Curve" } = "self" | return: bool | Pure |
| `getDuration` | `function` | self: { "Engine", "Curve" } = "self" | return: float | Pure |
| `evaluate` | `function` | self: { "Engine", "Curve" } = "self"; time: float | return: float | Pure |
