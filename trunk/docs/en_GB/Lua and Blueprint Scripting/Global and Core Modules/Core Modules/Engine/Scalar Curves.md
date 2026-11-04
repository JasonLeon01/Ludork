# Engine: Scalar Curves

## AnimSprite

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

`Engine.CurveInterpolation` exposes the read-only strings `CONSTANT = "constant"`, `LINEAR = "linear"` and `CUBIC = "cubic"` for a key’s `interpolation`. These values are shared by scalar and vector curves.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `value` | `float` | `0` | — |
| `interpolation` | `string` | `"linear"` | — |
| `arriveTangent` | `float` | `0` | — |
| `leaveTangent` | `float` | `0` | — |

## CurveData

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `type` | `string` | `"curve"` | — |
| `name` | `string` | — | — |
| `defaultValue` | `float` | `0` | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `{ "Engine", "CurveKey[]" }` | — | — |

## Curve

`Engine.CurveInfinityMode.CONSTANT` (`"constant"`) holds the endpoint value outside the key range; `LINEAR` (`"linear"`) extends the slope between the first two or last two keys. These read-only strings are used by `preInfinity` and `postInfinity` on scalar and vector curves and their data records. Infinity modes do not include cubic extrapolation.

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
