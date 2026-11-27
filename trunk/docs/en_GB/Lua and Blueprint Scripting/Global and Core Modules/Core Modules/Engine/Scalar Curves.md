# Engine: Scalar Curves

This page documents the compressed animation sprite player (`AnimSprite`) and scalar animation curve types (`CurveKey`, `CurveData`, `Curve`) under `Engine`. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods take no receiver.

## AnimSprite

Plays compressed `Engine.AnimationData` frame sequences and exposes duration, time-tag and completion queries.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setData` | `animationData: Engine.AnimationData` | — | — |
| `getDuration` | — | `float` | Pure. |
| `getVisualDuration` | — | `float` | Pure. |
| `getAllTimeTags` | — | `Engine.AnimationTimeTag[]` | Pure. |
| `isFinished` | — | `bool` | Pure. |
| `getFrameIndex` | — | `int` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `applyFrame` | `frameIndex: int` | — | — |

## CurveKey

Defines one keyframe on a scalar curve. `Engine.CurveInterpolation` exposes read-only strings `CONSTANT = "constant"`, `LINEAR = "linear"` and `CUBIC = "cubic"` for `interpolation`, shared by scalar and vector curves.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `value` | `float` | `0` | — |
| `interpolation` | `string` | `"linear"` | — |
| `arriveTangent` | `float` | `0` | — |
| `leaveTangent` | `float` | `0` | — |

## CurveData

Serialisable data record representing a scalar curve.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"curve"` | — |
| `name` | `string` | — | — |
| `defaultValue` | `float` | `0` | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.CurveKey[]` | — | — |

## Curve

Evaluatable scalar animation curve. `Engine.CurveInfinityMode` exposes read-only strings used by `preInfinity` and `postInfinity` on scalar and vector curves and data records: `CONSTANT` (`"constant"`) clamps to the endpoint value outside the key range, while `LINEAR` (`"linear"`) extends the slope between the first two or last two keys. Infinity modes do not include cubic extrapolation.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | — | — |
| `defaultValue` | `float` | `0` | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.CurveKey[]` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `fromData` | `data: Engine.CurveData` | `Engine.Curve` | Static. Pure. |
| `toData` | — | `Engine.CurveData` | Pure. |
| `isEmpty` | — | `bool` | Pure. |
| `getDuration` | — | `float` | Pure. |
| `evaluate` | `time: float` | `float` | Pure. |
