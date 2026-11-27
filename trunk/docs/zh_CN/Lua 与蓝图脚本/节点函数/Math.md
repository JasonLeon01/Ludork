# GlobalFunctions.Math

`GlobalFunctions.Math` 提供标量、向量、矩形、角度、随机数与运算符等纯函数蓝图节点，以及复合赋值执行节点。标量数学行为详见[数学扩展](<../../快速入门/Ludork Lua 进阶.md#数学扩展>)。

**元数据来源**：`Scripts/GlobalFunctions/Math_meta.lua`

## Math

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `BuildVector2f` | `x: float = 0`, `y: float = 0` | `value: sf.Vector2f` | 纯函数（Pure）。构造一个 `sf.Vector2f`。 |
| `BuildVector2i` | `x: int = 0`, `y: int = 0` | `value: sf.Vector2i` | 纯函数（Pure）。构造一个 `sf.Vector2i`。 |
| `BuildVector2u` | `x: int = 0`, `y: int = 0` | `value: sf.Vector2u` | 纯函数（Pure）。构造一个 `sf.Vector2u`。 |
| `BuildVector3f` | `x: float = 0`, `y: float = 0`, `z: float = 0` | `value: sf.Vector3f` | 纯函数（Pure）。构造一个 `sf.Vector3f`。 |
| `BuildVector3i` | `x: int = 0`, `y: int = 0`, `z: int = 0` | `value: sf.Vector3i` | 纯函数（Pure）。构造一个 `sf.Vector3i`。 |
| `IsNearZero` | `num: float`, `epsilon: float = 0.1` | `value: bool` | 纯函数（Pure）。检查 `num` 与 `0` 的差值是否在 `epsilon` 以内。 |
| `IsVector2NearZero` | `v: sf.Vector2f`, `epsilon: float = 0.1` | `value: bool` | 纯函数（Pure）。检查二维向量是否在 `epsilon` 容差内接近零向量。 |
| `IsVector3NearZero` | `v: sf.Vector3f`, `epsilon: float = 0.1` | `value: bool` | 纯函数（Pure）。检查三维向量是否在 `epsilon` 容差内接近零向量。 |
| `Vector2fRound` | `v: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。对 `v` 的各分量四舍五入。 |
| `Vector2fFloor` | `v: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。对 `v` 的各分量向下取整。 |
| `Vector2fCeil` | `v: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。对 `v` 的各分量向上取整。 |
| `ToVector2f` | `v: sf.Vector2i` | `value: sf.Vector2f` | 纯函数（Pure）。将 `sf.Vector2i` 转换为 `sf.Vector2f`。 |
| `ToVector2i` | `v: sf.Vector2f` | `value: sf.Vector2i` | 纯函数（Pure）。将 `sf.Vector2f` 转换为 `sf.Vector2i`。 |
| `ToVector2u` | `v: sf.Vector2f` | `value: sf.Vector2u` | 纯函数（Pure）。将 `sf.Vector2f` 转换为 `sf.Vector2u`。 |
| `ToVector3f` | `v: sf.Vector3i` | `value: sf.Vector3f` | 纯函数（Pure）。将 `sf.Vector3i` 转换为 `sf.Vector3f`。 |
| `ToVector3i` | `v: sf.Vector3f` | `value: sf.Vector3i` | 纯函数（Pure）。将 `sf.Vector3f` 转换为 `sf.Vector3i`。 |
| `ToIntRect` | `x: int = 0`, `y: int = 0`, `width: int = 32`, `height: int = 32` | `value: sf.IntRect` | 纯函数（Pure）。构造一个 `sf.IntRect`。 |
| `ToFloatRect` | `x: float = 0`, `y: float = 0`, `width: float = 32`, `height: float = 32` | `value: sf.FloatRect` | 纯函数（Pure）。构造一个 `sf.FloatRect`。 |
| `Clamp` | `value: float = 0`, `min_val: float = 0`, `max_val: float = 1` | `value: float` | 纯函数（Pure）。将 `value` 钳制在 `[min_val, max_val]` 区间内。 |
| `Lerp` | `a: float = 0`, `b: float = 1`, `t: float = 0.5` | `value: float` | 纯函数（Pure）。按比例 `t` 在 `a` 与 `b` 之间线性插值。 |
| `Sign` | `value: float = 0` | `value: int` | 纯函数（Pure）。返回 `value` 的符号。 |
| `InverseLerp` | `a: float = 0`, `b: float = 1`, `value: float = 0` | `value: float` | 纯函数（Pure）。计算 `value` 在 `a` 与 `b` 之间对应的插值参数。 |
| `Remap` | `value: float = 0`, `inMin: float = 0`, `inMax: float = 1`, `outMin: float = 0`, `outMax: float = 1` | `value: float` | 纯函数（Pure）。将 `value` 从 `[inMin, inMax]` 重映射到 `[outMin, outMax]`。 |
| `Smoothstep` | `edge0: float = 0`, `edge1: float = 1`, `value: float = 0` | `value: float` | 纯函数（Pure）。在 `edge0` 与 `edge1` 之间执行平滑埃尔米特（Hermite）插值。 |
| `MoveTowards` | `current: float = 0`, `target: float = 1`, `maxDelta: float = 0` | `value: float` | 纯函数（Pure）。使 `current` 向 `target` 逼近，单次变化量不超过 `maxDelta`。 |
| `Abs` | `value: float = 0` | `value: float` | 纯函数（Pure）。返回绝对值。 |
| `ToInt` | `value: float = 0` | `value: int` | 纯函数（Pure）。将浮点数转换为整数。 |
| `ToFloat` | `value: float = 0` | `value: float` | 纯函数（Pure）。将值转换为浮点数。 |
| `Max` | `values: any[] = {}` | `value: any` | 纯函数（Pure）。返回 `values` 中的最大元素。 |
| `Min` | `values: any[] = {}` | `value: any` | 纯函数（Pure）。返回 `values` 中的最小元素。 |
| `Sqrt` | `value: float = 0` | `value: float` | 纯函数（Pure）。返回 `value` 的平方根。 |
| `Pow` | `base: float = 1`, `exp: float = 2` | `value: float` | 纯函数（Pure）。计算 `base` 的 `exp` 次幂。 |
| `Vector2Distance` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: float` | 纯函数（Pure）。返回两个二维向量之间的欧氏距离。 |
| `Vector3Distance` | `v1: sf.Vector3f`, `v2: sf.Vector3f` | `value: float` | 纯函数（Pure）。返回两个三维向量之间的欧氏距离。 |
| `Vector2Dot` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: float` | 纯函数（Pure）。返回两个二维向量的点积。 |
| `Vector3Dot` | `v1: sf.Vector3f`, `v2: sf.Vector3f` | `value: float` | 纯函数（Pure）。返回两个三维向量的点积。 |
| `Vector2Cross` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: float` | 纯函数（Pure）。返回二维向量的标量叉积。 |
| `Vector3Cross` | `v1: sf.Vector3f`, `v2: sf.Vector3f` | `value: sf.Vector3f` | 纯函数（Pure）。返回两个三维向量的叉积。 |
| `Vector2Length` | `v: sf.Vector2f` | `value: float` | 纯函数（Pure）。返回二维向量的长度。 |
| `Vector3Length` | `v: sf.Vector3f` | `value: float` | 纯函数（Pure）。返回三维向量的长度。 |
| `Vector2LengthSquared` | `v: sf.Vector2f` | `value: float` | 纯函数（Pure）。返回二维向量长度的平方。 |
| `Vector3LengthSquared` | `v: sf.Vector3f` | `value: float` | 纯函数（Pure）。返回三维向量长度的平方。 |
| `Vector2Normalized` | `v: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。归一化二维向量。 |
| `Vector3Normalized` | `v: sf.Vector3f` | `value: sf.Vector3f` | 纯函数（Pure）。归一化三维向量。 |
| `GetAngle` | `v: sf.Vector2f` | `value: sf.Angle` | 纯函数（Pure）。返回二维向量 `v` 的极角。 |
| `GetAngleTo` | `v1: sf.Vector2f`, `v2: sf.Vector2f` | `value: sf.Angle` | 纯函数（Pure）。返回从 `v1` 到 `v2` 的有符号夹角。 |
| `AsDegrees` | `angle: sf.Angle` | `value: float` | 纯函数（Pure）。将 `sf.Angle` 转换为角度值。 |
| `AsRadians` | `angle: sf.Angle` | `value: float` | 纯函数（Pure）。将 `sf.Angle` 转换为弧度值。 |
| `Vector2ComponentWiseDiv` | `v: sf.Vector2f`, `div: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。将 `v` 按分量除以 `div`。 |
| `Vector2ComponentWiseMul` | `v: sf.Vector2f`, `mul: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。将 `v` 按分量乘以 `mul`。 |
| `Vector2Perpendicular` | `v: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。返回垂直于 `v` 的向量。 |
| `Vector2ProjectedOnto` | `v: sf.Vector2f`, `axis: sf.Vector2f` | `value: sf.Vector2f` | 纯函数（Pure）。将 `v` 投影到 `axis` 上。 |
| `Vector2RotatedBy` | `v: sf.Vector2f`, `phi: sf.Angle` | `value: sf.Vector2f` | 纯函数（Pure）。将 `v` 旋转 `phi` 角度。 |
| `DegreesToAngle` | `degrees_: float = 0` | `value: sf.Angle` | 纯函数（Pure）。由角度值创建 `sf.Angle`。 |
| `RadiansToAngle` | `radians_: float = 0` | `value: float` | 纯函数（Pure）。由弧度值创建角度（元数据声明返回类型为 `float`）。 |
| `RandomInt` | `min_val: int = 0`, `max_val: int = 100` | `value: int` | 纯函数（Pure）。返回 `[min_val, max_val]` 范围内的随机整数。 |
| `RandomFloat` | `min_val: float = 0`, `max_val: float = 1` | `value: float` | 纯函数（Pure）。返回 `[min_val, max_val]` 范围内的随机浮点数。 |
| `GCD` | `a: int = 1`, `b: int = 1` | `value: int` | 纯函数（Pure）。返回 `a` 与 `b` 的最大公约数。 |
| `LCM` | `a: int = 1`, `b: int = 1` | `value: int` | 纯函数（Pure）。返回 `a` 与 `b` 的最小公倍数。 |
| `ADD` | `a: any = 0`, `b: any = 0` | `value: any` | 纯函数（Pure）。显示名称：`+`。 |
| `SUB` | `a: any = 0`, `b: any = 0` | `value: any` | 纯函数（Pure）。显示名称：`-`。 |
| `MUL` | `a: any = 1`, `b: any = 1` | `value: any` | 纯函数（Pure）。显示名称：`*`。 |
| `DIV` | `a: any = 1`, `b: any = 1` | `value: any` | 纯函数（Pure）。显示名称：`/`。 |
| `MOD` | `a: any = 0`, `b: any = 1` | `value: any` | 纯函数（Pure）。显示名称：`%`。 |
| `POW` | `a: any = 1`, `b: any = 1` | `value: any` | 纯函数（Pure）。显示名称：`**`。 |
| `EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | 纯函数（Pure）。显示名称：`==`。 |
| `NOT_EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | 纯函数（Pure）。显示名称：`!=`。 |
| `LESS` | `a: any = 0`, `b: any = 0` | `value: bool` | 纯函数（Pure）。显示名称：`<`。 |
| `LESS_EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | 纯函数（Pure）。显示名称：`<=`。 |
| `GREATER` | `a: any = 0`, `b: any = 0` | `value: bool` | 纯函数（Pure）。显示名称：`>`。 |
| `GREATER_EQUALS` | `a: any = 0`, `b: any = 0` | `value: bool` | 纯函数（Pure）。显示名称：`>=`。 |
| `AND` | `a: bool = false`, `b: bool = false` | `value: bool` | 纯函数（Pure）。逻辑与（AND）。 |
| `OR` | `a: bool = false`, `b: bool = false` | `value: bool` | 纯函数（Pure）。逻辑或（OR）。 |
| `NOT` | `a: bool = false` | `value: bool` | 纯函数（Pure）。逻辑非（NOT）。 |
| `XOR` | `a: bool = false`, `b: bool = false` | `value: bool` | 纯函数（Pure）。逻辑异或（XOR）。 |
| `NAND` | `a: bool = false`, `b: bool = false` | `value: bool` | 纯函数（Pure）。逻辑与非（NAND）。 |
| `NOR` | `a: bool = false`, `b: bool = false` | `value: bool` | 纯函数（Pure）。逻辑或非（NOR）。 |
| `XNOR` | `a: bool = false`, `b: bool = false` | `value: bool` | 纯函数（Pure）。逻辑同或（XNOR）。 |
| `IADD` | `a: any`, `b: any = 1` | — | 显示名称：`+=`。对引用执行原地加法。 |
| `ISUB` | `a: any`, `b: any = 1` | — | 显示名称：`-=`。对引用执行原地减法。 |
| `IMUL` | `a: any`, `b: any = 2` | — | 显示名称：`*=`。对引用执行原地乘法。 |
| `IDIV` | `a: any`, `b: any = 2` | — | 显示名称：`/=`。对引用执行原地除法。 |
| `IMOD` | `a: any`, `b: any = 2` | — | 显示名称：`%=`。对引用执行原地取模。 |
| `IPOW` | `a: any`, `b: any = 2` | — | 显示名称：`**=`。对引用执行原地乘方。 |
