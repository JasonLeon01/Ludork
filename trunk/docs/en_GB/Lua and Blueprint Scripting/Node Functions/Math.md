# GlobalFunctions.Math

`GlobalFunctions.Math` provides pure scalar, vector, rectangle, angle, random and operator Blueprint nodes, together with compound-assignment execution nodes. For scalar math behaviour, see [Math extensions](<../../Getting Started/Ludork Lua Advanced.md#math-extensions>).

**Metadata source:** `Scripts/GlobalFunctions/Math_meta.lua`

## Math

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `BuildVector2f` | `x: float = 0`, `y: float = 0` | `value: sf.Vector2f` | Pure. Constructs an `sf.Vector2f`. |
| `BuildVector2i` | `x: int = 0`, `y: int = 0` | `value: sf.Vector2i` | Pure. Constructs an `sf.Vector2i`. |
| `BuildVector2u` | `x: int = 0`, `y: int = 0` | `value: sf.Vector2u` | Pure. Constructs an `sf.Vector2u`. |
| `BuildVector3f` | `x: float = 0`, `y: float = 0`, `z: float = 0` | `value: sf.Vector3f` | Pure. Constructs an `sf.Vector3f`. |
| `BuildVector3i` | `x: int = 0`, `y: int = 0`, `z: int = 0` | `value: sf.Vector3i` | Pure. Constructs an `sf.Vector3i`. |
| `IsNearZero` | `num: float`, `epsilon: float = 0.1` | `value: bool` | Pure. Checks whether `num` is within `epsilon` of `0`. |
| `IsVector2NearZero` | `v: sf.Vector2f`, `epsilon: float = 0.1` | `value: bool` | Pure. Checks whether a 2D vector is near zero within `epsilon`. |
| `IsVector3NearZero` | `v: sf.Vector3f`, `epsilon: float = 0.1` | `value: bool` | Pure. Checks whether a 3D vector is near zero within `epsilon`. |
| `Vector2fRound` | `v: sf.Vector2f` | `value: sf.Vector2f` | Pure. Rounds each component of `v`. |
| `Vector2fFloor` | `v: sf.Vector2f` | `value: sf.Vector2f` | Pure. Floors each component of `v`. |
| `Vector2fCeil` | `v: sf.Vector2f` | `value: sf.Vector2f` | Pure. Ceils each component of `v`. |
| `ToVector2f` | `v: sf.Vector2i` | `value: sf.Vector2f` | Pure. Converts `sf.Vector2i` to `sf.Vector2f`. |
| `ToVector2i` | `v: sf.Vector2f` | `value: sf.Vector2i` | Pure. Converts `sf.Vector2f` to `sf.Vector2i`. |
| `ToVector2u` | `v: sf.Vector2f` | `value: sf.Vector2u` | Pure. Converts `sf.Vector2f` to `sf.Vector2u`. |
| `ToVector3f` | `v: sf.Vector3i` | `value: sf.Vector3f` | Pure. Converts `sf.Vector3i` to `sf.Vector3f`. |
| `ToVector3i` | `v: sf.Vector3f` | `value: sf.Vector3i` | Pure. Converts `sf.Vector3f` to `sf.Vector3i`. |
| `ToIntRect` | `x: int = 0`, `y: int = 0`, `width: int = 32`, `height: int = 32` | `value: sf.IntRect` | Pure. Constructs an `sf.IntRect`. |
| `ToFloatRect` | `x: float = 0`, `y: float = 0`, `width: float = 32`, `height: float = 32` | `value: sf.FloatRect` | Pure. Constructs an `sf.FloatRect`. |
| `Clamp` | `value: float = 0`, `min_val: float = 0`, `max_val: float = 1` | `value: float` | Pure. Clamps `value` to `[min_val, max_val]`. |
| `Lerp` | `a: float = 0`, `b: float = 1`, `t: float = 0.5` | `value: float` | Pure. Linearly interpolates between `a` and `b` by `t`. |
| `Sign` | `value: float = 0` | `value: int` | Pure. Returns the sign of `value`. |
| `InverseLerp` | `a: float = 0`, `b: float = 1`, `value: float = 0` | `value: float` | Pure. Returns the interpolation parameter of `value` between `a` and `b`. |
| `Remap` | `value: float = 0`, `inMin: float = 0`, `inMax: float = 1`, `outMin: float = 0`, `outMax: float = 1` | `value: float` | Pure. Remaps `value` from `[inMin, inMax]` to `[outMin, outMax]`. |
| `Smoothstep` | `edge0: float = 0`, `edge1: float = 1`, `value: float = 0` | `value: float` | Pure. Performs smooth Hermite interpolation between `edge0` and `edge1`. |
| `MoveTowards` | `current: float = 0`, `target: float = 1`, `maxDelta: float = 0` | `value: float` | Pure. Moves `current` towards `target` by at most `maxDelta`. |
| `Abs` | `value: float = 0` | `value: float` | Pure. Returns the absolute value. |
| `ToInt` | `value: float = 0` | `value: int` | Pure. Converts a float to an integer. |
| `ToFloat` | `value: float = 0` | `value: float` | Pure. Converts a value to a float. |
| `Max` | `values: any[] = {}` | `value: any` | Pure. Returns the maximum element of `values`. |
| `Min` | `values: any[] = {}` | `value: any` | Pure. Returns the minimum element of `values`. |
| `Sqrt` | `value: float = 0` | `value: float` | Pure. Returns the square root of `value`. |
| `Pow` | `base: float = 1`, `exp: float = 2` | `value: float` | Pure. Raises `base` to the power of `exp`. |
| `Vector2Distance` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: float` | Pure. Returns the Euclidean distance between two 2D vectors. |
| `Vector3Distance` | `v1: sf.Vector3f`, `v2: sf.Vector3f` | `value: float` | Pure. Returns the Euclidean distance between two 3D vectors. |
| `Vector2Dot` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: float` | Pure. Returns the dot product of two 2D vectors. |
| `Vector3Dot` | `v1: sf.Vector3f`, `v2: sf.Vector3f` | `value: float` | Pure. Returns the dot product of two 3D vectors. |
| `Vector2Cross` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: float` | Pure. Returns the 2D scalar cross product. |
| `Vector3Cross` | `v1: sf.Vector3f`, `v2: sf.Vector3f` | `value: sf.Vector3f` | Pure. Returns the 3D cross product. |
| `Vector2Length` | `v: sf.Vector2f` | `value: float` | Pure. Returns the length of a 2D vector. |
| `Vector3Length` | `v: sf.Vector3f` | `value: float` | Pure. Returns the length of a 3D vector. |
| `Vector2LengthSquared` | `v: sf.Vector2f` | `value: float` | Pure. Returns the squared length of a 2D vector. |
| `Vector3LengthSquared` | `v: sf.Vector3f` | `value: float` | Pure. Returns the squared length of a 3D vector. |
| `Vector2Normalized` | `v: sf.Vector2f` | `value: sf.Vector2f` | Pure. Normalises a 2D vector. |
| `Vector3Normalized` | `v: sf.Vector3f` | `value: sf.Vector3f` | Pure. Normalises a 3D vector. |
| `GetAngle` | `v: sf.Vector2f` | `value: sf.Angle` | Pure. Returns the polar angle of `v`. |
| `GetAngleTo` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: sf.Angle` | Pure. Returns the signed angle from `v1` to `v2`. |
| `AsDegrees` | `angle: sf.Angle` | `value: float` | Pure. Converts an `sf.Angle` to degrees. |
| `AsRadians` | `angle: sf.Angle` | `value: float` | Pure. Converts an `sf.Angle` to radians. |
| `Vector2ComponentWiseDiv` | `v: sf.Vector2f`, `div: sf.Vector2f` | `value: sf.Vector2f` | Pure. Divides `v` component-wise by `div`. |
| `Vector2ComponentWiseMul` | `v: sf.Vector2f`, `mul: sf.Vector2f` | `value: sf.Vector2f` | Pure. Multiplies `v` component-wise by `mul`. |
| `Vector2Perpendicular` | `v: sf.Vector2f` | `value: sf.Vector2f` | Pure. Returns a vector perpendicular to `v`. |
| `Vector2ProjectedOnto` | `v: sf.Vector2f`, `axis: sf.Vector2f` | `value: sf.Vector2f` | Pure. Projects `v` onto `axis`. |
| `Vector2RotatedBy` | `v: sf.Vector2f`, `phi: sf.Angle` | `value: sf.Vector2f` | Pure. Rotates `v` by `phi`. |
| `DegreesToAngle` | `degrees_: float = 0` | `value: sf.Angle` | Pure. Creates an `sf.Angle` from degrees. |
| `RadiansToAngle` | `radians_: float = 0` | `value: float` | Pure. Creates an angle from radians (metadata declares return type `float`). |
| `RandomInt` | `min_val: int = 0`, `max_val: int = 100` | `value: int` | Pure. Returns a random integer in `[min_val, max_val]`. |
| `RandomFloat` | `min_val: float = 0`, `max_val: float = 1` | `value: float` | Pure. Returns a random float in `[min_val, max_val]`. |
| `GCD` | `a: int = 1`, `b: int = 1` | `value: int` | Pure. Returns the greatest common divisor of `a` and `b`. |
| `LCM` | `a: int = 1`, `b: int = 1` | `value: int` | Pure. Returns the least common multiple of `a` and `b`. |
| `ADD` | `a: any = 0`, `b: any = 0` | `value: any` | Pure. Display name: `+`. |
| `SUB` | `a: any = 0`, `b: any = 0` | `value: any` | Pure. Display name: `-`. |
| `MUL` | `a: any = 1`, `b: any = 1` | `value: any` | Pure. Display name: `*`. |
| `DIV` | `a: any = 1`, `b: any = 1` | `value: any` | Pure. Display name: `/`. |
| `MOD` | `a: any = 0`, `b: any = 1` | `value: any` | Pure. Display name: `%`. |
| `POW` | `a: any = 1`, `b: any = 1` | `value: any` | Pure. Display name: `**`. |
| `EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | Pure. Display name: `==`. |
| `NOT_EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | Pure. Display name: `!=`. |
| `LESS` | `a: any = 0`, `b: any = 0` | `value: bool` | Pure. Display name: `<`. |
| `LESS_EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | Pure. Display name: `<=`. |
| `GREATER` | `a: any = 0`, `b: any = 0` | `value: bool` | Pure. Display name: `>`. |
| `GREATER_EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | Pure. Display name: `>=`. |
| `AND` | `a: bool = false`, `b: bool = false` | `value: bool` | Pure. Logical AND. |
| `OR` | `a: bool = false`, `b: bool = false` | `value: bool` | Pure. Logical OR. |
| `NOT` | `a: bool = false` | `value: bool` | Pure. Logical NOT. |
| `XOR` | `a: bool = false`, `b: bool = false` | `value: bool` | Pure. Logical XOR. |
| `NAND` | `a: bool = false`, `b: bool = false` | `value: bool` | Pure. Logical NAND. |
| `NOR` | `a: bool = false`, `b: bool = false` | `value: bool` | Pure. Logical NOR. |
| `XNOR` | `a: bool = false`, `b: bool = false` | `value: bool` | Pure. Logical XNOR. |
| `IADD` | `a: any`, `b: any = 1` | — | Display name: `+=`. In-place addition on a reference. |
| `ISUB` | `a: any`, `b: any = 1` | — | Display name: `-=`. In-place subtraction on a reference. |
| `IMUL` | `a: any`, `b: any = 2` | — | Display name: `*=`. In-place multiplication on a reference. |
| `IDIV` | `a: any`, `b: any = 2` | — | Display name: `/=`. In-place division on a reference. |
| `IMOD` | `a: any`, `b: any = 2` | — | Display name: `%=`. In-place modulo on a reference. |
| `IPOW` | `a: any`, `b: any = 2` | — | Display name: `**=`. In-place exponentiation on a reference. |
