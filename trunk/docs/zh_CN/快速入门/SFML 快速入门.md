# SFML 快速入门

SFML 提供 Ludork 的图形、音频、窗口、输入与几何类型。LuaSF 将其暴露为全局 `sf` 表，项目脚本无需调用 `require` 即可直接使用。

## 官方文档

有关成员详情，请参阅 [SFML 3.1.0 API 参考](<https://www.sfml-dev.org/documentation/3.1.0/>)；有关图形与音频概念，请参阅 [SFML 3.1 教程](<https://www.sfml-dev.org/tutorials/3.1/>)。Ludork 内置 **SFML 3.1.0**，其构造方式与 SFML 2 不同。

## Ludork 与 SFML 的分工

图形数值与资源使用 `sf`；地图、Actor、蓝图、场景、UI 与项目数据则使用 `Engine`、`GlobalCore` 与 `GlobalFunctions`。运行时接管窗口、帧循环与 UI 树，因此项目绘制通常通过 Engine 控件与场景完成，无需另行创建 `sf.RenderWindow`。

## sf 表

`sf` 包含以下类、枚举与辅助函数：

| `sf` 分组 | 成员 |
|---|---|
| 向量与矩阵 | `sf.Vector2i`、`sf.Vector2u`、`sf.Vector2f`、`sf.Vector2b`、`sf.Vector3i`、`sf.Vector3u`、`sf.Vector3f`、`sf.Vector3b`、`sf.Vector4i`、`sf.Vector4f`、`sf.Vector4b`、`sf.Mat3`、`sf.Mat4`，以及 GLSL 别名 `sf.Vec2`、`sf.Vec3`、`sf.Vec4`、`sf.Ivec2`、`sf.Ivec3`、`sf.Ivec4`、`sf.Bvec2`、`sf.Bvec3`、`sf.Bvec4` |
| 几何 | `sf.Color`、`sf.IntRect`、`sf.FloatRect`、`sf.Angle`、`sf.Time`、`sf.Transform` |
| 图形 | `sf.Texture`、`sf.Image`、`sf.Font`、`sf.Text`、`sf.Shader`、`sf.RenderStates`、`sf.RenderTarget`、`sf.RenderTexture`、`sf.RenderWindow`、`sf.VertexArray`、`sf.VertexBuffer`、`sf.Vertex`、`sf.Shape`、`sf.CircleShape`、`sf.ConvexShape`、`sf.RectangleShape`、`sf.Sprite`、`sf.View` |
| 音频 | `sf.SoundBuffer`、`sf.Sound`、`sf.Music`、`sf.SoundStream`、`sf.SoundSource`、`sf.Listener`、`sf.SoundRecorder`、`sf.SoundChannel`、`sf.PlaybackDevice` |
| 窗口与输入 | `sf.Keyboard`、`sf.Mouse`、`sf.Joystick`、`sf.Touch`、`sf.Sensor`、`sf.Clipboard`、`sf.Cursor`、`sf.VideoMode`、`sf.Event` 及其 `sf.Event_*` 变体 |
| 系统 | `sf.Clock`、`sf.sleep`、`sf.Version`、`sf.InputStream`、`sf.FileInputStream`、`sf.MemoryInputStream` |
| 网络 | `sf.IpAddress`、`sf.Dns`、`sf.Http`、`sf.Ftp`、`sf.Sftp`、`sf.TcpSocket`、`sf.TcpListener`、`sf.UdpSocket`、`sf.SocketSelector`、`sf.Packet` |

常用辅助函数直接位于 `sf` 下：`sf.seconds`、`sf.milliseconds` 与 `sf.microseconds` 用于构造 `sf.Time`，`sf.degrees` 与 `sf.radians` 用于构造 `sf.Angle`。`sf.BlendAlpha`、`sf.BlendAdd` 等命名混合模式也作为 `sf` 的字段提供。

## 值类型

几何值为原生 userdata，区别于 Lua table 与 Standard 容器。

通过 `new` 构造实例：

```lua
local cell = sf.Vector2i.new(4, 7)
local speed = sf.Vector2f.new(1.5, -2.5)
local limit = sf.Vector2u.new(640, 480)
local tint = sf.Color.new(255, 255, 255, 128)
local area = sf.IntRect.new(0, 0, 32, 32)
```

`x` 与 `y` 为可读写字段，`Vector3*` 与 `Vector4*` 另提供 `z` 与 `w`：

```lua
local position = sf.Vector2i.new(0, 0)
position.x = position.x + 1
position.y = 4

local amount = sf.Vector3f.new(1.0, 0.5, 0.0)
local offset = sf.Vector4f.new(0.0, 0.0, 0.5, 1.0)
```

二维向量支持算术运算：

```lua
local start = sf.Vector2f.new(64.0, 64.0)
local step = sf.Vector2f.new(16.0, 0.0)

local moved = start + step * 2.0   -- Vector2f(96, 64)
local isSame = moved == sf.Vector2f.new(96.0, 64.0) -- true
```

- **运算符**：二维向量支持 `+`、`-`、一元 `-` 与 `==`，以及与标量数字的 `*` 和 `/`。未定义 `<` 等大小排序比较，向量间乘法通过方法而非运算符执行。
- **方法**：三种 `Vector2` 类型均提供 `lengthSquared`、`perpendicular`、`dot`、`cross`、`componentWiseMul`、`componentWiseDiv` 及 `unpack`（将各分量作为多个返回值返回）。`sf.Vector2f` 额外提供 `length`、`normalized`、`angle`、`angleTo`、`rotatedBy` 与 `projectedOnto`，均以 `sf.Angle`（而非度数）为角度单位。

`sf.Vector2f.new` 也接受长度与 `sf.Angle` 参数，按极坐标构造向量：

```lua
local forward = sf.Vector2f.new(1.0, sf.degrees(90)) -- Vector2f(0, 1)
```

### 颜色、矩形与时间

`sf.Color` 包含取值范围为 0–255 的 `r`、`g`、`b` 与 `a` 分量。四参数构造函数依次接收红、绿、蓝与透明度，较短形式对省略分量采用默认值：

```lua
local solid = sf.Color.new(32, 96, 160)
local veil = sf.Color.new(0, 0, 0, 160)
```

预设颜色作为 `sf.Color` 的字段提供：`Black`、`White`、`Red`、`Green`、`Blue`、`Yellow`、`Cyan`、`Magenta` 与 `Transparent`。`toInteger` 返回打包的 32 位整数。`sf.Color` 未定义运算符，不可直接相加或比较，需要时应读取并比较各个分量。

`sf.IntRect.new` 与 `sf.FloatRect.new` 接受“位置 + 尺寸”或四个数字。后两个数字表示宽度与高度，而非右下角坐标：

```lua
local tile = sf.IntRect.new(sf.Vector2i.new(0, 0), sf.Vector2i.new(32, 32))
local same = sf.IntRect.new(0, 0, 32, 32)
local sub = sf.IntRect.new(32, 0, 32, 32)

local origin = tile.position      -- Vector2i(0, 0)
local height = tile.size.y        -- 32
local inside = tile.contains(sf.Vector2i.new(16, 16)) -- true
local overlap = tile.findIntersection(sub) -- 返回 sf.IntRect；不相交时为 nil
```

`sf.Angle` 表示旋转角度而非原始数字，必须通过 `sf.degrees` 或 `sf.radians` 构造：

```lua
local facing = sf.degrees(90)
local radians = sf.radians(math.pi / 2)

local degrees = facing.asDegrees()
local wrapped = sf.degrees(450).wrapSigned().asDegrees() -- 90
```

`wrapSigned` 将角度归一化至 `[-180°, 180°)`，`wrapUnsigned` 归一化至 `[0°, 360°)`；未归一化的角度同样有效。`sf.Angle.new()` 与 `sf.Angle.Zero` 均表示零度。

`sf.Time` 表示时长，通过 `sf.seconds`、`sf.milliseconds` 或 `sf.microseconds` 构造：

```lua
local cooldown = sf.seconds(0.75)

local milliseconds = cooldown.asMilliseconds() -- 750
```

`sf.Time.new()` 为零值；`sf.sleep` 将当前调用线程暂停指定时长。

## 对象类型

Lua 中的图形对象完整对应 SFML 的继承层次：

```text
sf.Drawable
sf.Transformable
sf.Shape : sf.Drawable, sf.Transformable
sf.CircleShape, sf.ConvexShape, sf.RectangleShape : sf.Shape
sf.Sprite : sf.Drawable, sf.Transformable
sf.Text : sf.Drawable, sf.Transformable
```

所有 `sf.Transformable` 均提供 `setPosition`、`getPosition`（`sf.Vector2f`）、`setRotation`、`getRotation`（`sf.Angle`）、`setScale`、`getScale`、`setOrigin`、`getOrigin`（`sf.Vector2f`），以及 `move`、`rotate`、`scale` 与 `getTransform`。

```lua
local cell = sf.RectangleShape.new(sf.Vector2f.new(32.0, 32.0))

cell:setPosition(sf.Vector2f.new(64.0, 64.0))
cell:setOrigin(sf.Vector2f.new(16.0, 16.0))
cell:setRotation(sf.degrees(45))
cell:setScale(sf.Vector2f.new(2.0, 2.0))
cell:move(sf.Vector2f.new(8.0, 0.0))

local bounds = cell:getGlobalBounds()
```

`sf.RectangleShape.new` 接收尺寸，`sf.CircleShape.new` 接收半径与顶点数，`sf.ConvexShape.new` 接收顶点数，`sf.Text.new` 接收字体、字符串与字号。形状类还提供 `setFillColor`、`setOutlineColor`（`sf.Color`）与 `setOutlineThickness`（数字）。

抽象基类（`sf.Drawable`、`sf.Shape`、`sf.RenderTarget`、`sf.InputStream`、`sf.SoundSource`、`sf.SoundStream`、`sf.SoundRecorder`、`sf.SoundFileReader`、`sf.SoundFileWriter` 与 `sf.Socket`）以表的形式存在但不含 `new`，无法直接构造。请改为构造具体子类，仅将基类用作参数或返回值类型。

`sf.Drawable` 是唯一可由 Lua 直接实现的基类：调用 `sf.LuaDrawable.new(drawCallback)` 可创建在 Lua 中执行 `draw` 的可绘制对象。

事件遵循反向模式：`sf.Event_Closed`、`sf.Event_Resized` 等载荷类型可直接构造，而 `sf.Event.new` 需接收其中之一作为参数。项目代码通常只需接收事件，无需自行构造。

## 在项目中使用 SFML

位置、尺寸与矩形在 Engine API 中均以 `sf` 值传递，因此格子坐标与世界坐标之间的换算可直接使用常规向量运算：

```lua
local Engine = require("Engine")
local Player = require("GlobalFunctions.Player")

local CELL_SIZE = Engine.GetCellSize()

local function cellToWorld(cellX, cellY)
    return sf.Vector2f.new(cellX * CELL_SIZE, cellY * CELL_SIZE)
end

local player = Player.GetPlayer()
if player ~= nil then
    local position = player:getPosition()
    local cell = sf.Vector2i.new(
        math.floor(position.x / CELL_SIZE),
        math.floor(position.y / CELL_SIZE)
    )
end
```

传参前请确认声明类型：要求 `sf.Vector2f` 的 API 会拒绝 `sf.Vector2i`、Standard `tuple` 或普通 table，不会隐式转换。

JSON 与元数据仅存储纯数据，而 `sf` 值为原生 userdata，两者之间需显式转换。Game 项目的存档编解码器将 `sf.Vector2i` 写为 `{ x, y }` 并在加载时重建，其记录归一化器则将 `sf.Vector2i`、`sf.Vector3i`、`sf.IntRect`、`sf.FloatRect` 与 `sf.Color` 转为数字数组。向数据目标写入时应存储原始分量，使用时再重新构造 `sf` 值。

## 延伸阅读

`Scripts/stub/LuaSF.d.lua` 包含完整的 Lua 声明与 SFML 注释，可供 EmmyLua 索引以提供代码补全与悬浮提示。各成员名称与参数顺序与上文链接的 SFML 官方 C++ 参考基本一致。
