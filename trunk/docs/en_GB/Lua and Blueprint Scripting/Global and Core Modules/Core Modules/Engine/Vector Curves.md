# Engine: Vector Curves

This page documents two-, three- and four-component animation curve types (`Vector2Curve`, `Vector3Curve`, `Vector4Curve`) and their key and serialisable data records under `Engine`. Vector curves share the [interpolation constants](<Scalar Curves.md#curvekey>) and [infinity-mode constants](<Scalar Curves.md#curve>) defined for scalar curves. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods take no receiver.

## Vector2CurveKey

Defines one two-component keyframe on a `Vector2Curve`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `value` | `float[]` | — | — |
| `interpolation` | `string` | `"linear"` | — |
| `arriveTangent` | `float[]` | — | — |
| `leaveTangent` | `float[]` | — | — |

## Vector2CurveData

Serialisable data record representing a two-component vector curve.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"vector2Curve"` | — |
| `name` | `string` | — | — |
| `defaultValue` | `float[]` | — | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.Vector2CurveKey[]` | — | — |

## Vector2Curve

Evaluatable two-component vector animation curve.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | — | — |
| `defaultValue` | `float[]` | — | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.Vector2CurveKey[]` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `fromData` | `data: Engine.Vector2CurveData` | `Engine.Vector2Curve` | Static. Pure. |
| `toData` | — | `Engine.Vector2CurveData` | Pure. |
| `isEmpty` | — | `bool` | Pure. |
| `getDuration` | — | `float` | Pure. |
| `evaluate` | `time: float` | `float[]` | Pure. |

## Vector3CurveKey

Defines one three-component keyframe on a `Vector3Curve`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `value` | `float[]` | — | — |
| `interpolation` | `string` | `"linear"` | — |
| `arriveTangent` | `float[]` | — | — |
| `leaveTangent` | `float[]` | — | — |

## Vector3CurveData

Serialisable data record representing a three-component vector curve.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"vector3Curve"` | — |
| `name` | `string` | — | — |
| `defaultValue` | `float[]` | — | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.Vector3CurveKey[]` | — | — |

## Vector3Curve

Evaluatable three-component vector animation curve.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | — | — |
| `defaultValue` | `float[]` | — | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.Vector3CurveKey[]` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `fromData` | `data: Engine.Vector3CurveData` | `Engine.Vector3Curve` | Static. Pure. |
| `toData` | — | `Engine.Vector3CurveData` | Pure. |
| `isEmpty` | — | `bool` | Pure. |
| `getDuration` | — | `float` | Pure. |
| `evaluate` | `time: float` | `float[]` | Pure. |

## Vector4CurveKey

Defines one four-component keyframe on a `Vector4Curve`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `value` | `float[]` | — | — |
| `interpolation` | `string` | `"linear"` | — |
| `arriveTangent` | `float[]` | — | — |
| `leaveTangent` | `float[]` | — | — |

## Vector4CurveData

Serialisable data record representing a four-component vector curve.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"vector4Curve"` | — |
| `name` | `string` | — | — |
| `defaultValue` | `float[]` | — | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.Vector4CurveKey[]` | — | — |

## Vector4Curve

Evaluatable four-component vector animation curve (commonly used for RGBA colour gradients).

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | — | — |
| `defaultValue` | `float[]` | — | — |
| `preInfinity` | `string` | `"constant"` | — |
| `postInfinity` | `string` | `"constant"` | — |
| `keys` | `Engine.Vector4CurveKey[]` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `fromData` | `data: Engine.Vector4CurveData` | `Engine.Vector4Curve` | Static. Pure. |
| `toData` | — | `Engine.Vector4CurveData` | Pure. |
| `isEmpty` | — | `bool` | Pure. |
| `getDuration` | — | `float` | Pure. |
| `evaluate` | `time: float` | `float[]` | Pure. |
