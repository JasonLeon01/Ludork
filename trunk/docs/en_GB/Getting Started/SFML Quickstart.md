# SFML Quickstart

SFML supplies Ludork's graphics, audio, window, input and geometry types. LuaSF exposes it as the global `sf` table, available to project scripts without `require`.

## Official documentation

Use the [SFML 3.1.0 API reference](<https://www.sfml-dev.org/documentation/3.1.0/>) for member details and the [3.1 tutorials](<https://www.sfml-dev.org/tutorials/3.1/>) for graphics and audio concepts. Ludork vendors **SFML 3.1.0**; SFML 2 uses a different constructor style.

## What Ludork provides and what SFML provides

Use `sf` for graphics values and resources; use `Engine`, `GlobalCore` and `GlobalFunctions` for maps, Actors, Blueprints, Scenes, UI and project data. The runtime owns the window, frame loop and UI tree, so project drawing normally uses Engine controls and Scenes rather than a separate `sf.RenderWindow`.

## The sf table

`sf` contains classes, enums and helper functions:

| `sf` group | Members |
|---|---|
| Vector and matrix | `sf.Vector2i`, `sf.Vector2u`, `sf.Vector2f`, `sf.Vector2b`, `sf.Vector3i`, `sf.Vector3u`, `sf.Vector3f`, `sf.Vector3b`, `sf.Vector4i`, `sf.Vector4f`, `sf.Vector4b`, `sf.Mat3`, `sf.Mat4`, and the GLSL aliases `sf.Vec2`, `sf.Vec3`, `sf.Vec4`, `sf.Ivec2`, `sf.Ivec3`, `sf.Ivec4`, `sf.Bvec2`, `sf.Bvec3`, `sf.Bvec4` |
| Geometry | `sf.Color`, `sf.IntRect`, `sf.FloatRect`, `sf.Angle`, `sf.Time`, `sf.Transform` |
| Graphics | `sf.Texture`, `sf.Image`, `sf.Font`, `sf.Text`, `sf.Shader`, `sf.RenderStates`, `sf.RenderTarget`, `sf.RenderTexture`, `sf.RenderWindow`, `sf.VertexArray`, `sf.VertexBuffer`, `sf.Vertex`, `sf.Shape`, `sf.CircleShape`, `sf.ConvexShape`, `sf.RectangleShape`, `sf.Sprite`, `sf.View` |
| Audio | `sf.SoundBuffer`, `sf.Sound`, `sf.Music`, `sf.SoundStream`, `sf.SoundSource`, `sf.Listener`, `sf.SoundRecorder`, `sf.SoundChannel`, `sf.PlaybackDevice` |
| Window and input | `sf.Keyboard`, `sf.Mouse`, `sf.Joystick`, `sf.Touch`, `sf.Sensor`, `sf.Clipboard`, `sf.Cursor`, `sf.VideoMode`, `sf.Event` and its `sf.Event_*` variants |
| System | `sf.Clock`, `sf.sleep`, `sf.Version`, `sf.InputStream`, `sf.FileInputStream`, `sf.MemoryInputStream` |
| Network | `sf.IpAddress`, `sf.Dns`, `sf.Http`, `sf.Ftp`, `sf.Sftp`, `sf.TcpSocket`, `sf.TcpListener`, `sf.UdpSocket`, `sf.SocketSelector`, `sf.Packet` |

Helpers sit directly on `sf`: `sf.seconds`, `sf.milliseconds` and `sf.microseconds` build an `sf.Time`, while `sf.degrees` and `sf.radians` build an `sf.Angle`. Named blend modes such as `sf.BlendAlpha` and `sf.BlendAdd` are also fields on `sf`.

## Value types

Geometry values are native userdata, distinct from Lua tables and Standard containers.

Construct them with `new`:

```lua
local cell = sf.Vector2i.new(4, 7)
local speed = sf.Vector2f.new(1.5, -2.5)
local limit = sf.Vector2u.new(640, 480)
local tint = sf.Color.new(255, 255, 255, 128)
local area = sf.IntRect.new(0, 0, 32, 32)
```

`x` and `y` are readable and writable fields, and `Vector3*` and `Vector4*` add `z` and `w`:

```lua
local position = sf.Vector2i.new(0, 0)
position.x = position.x + 1
position.y = 4

local amount = sf.Vector3f.new(1.0, 0.5, 0.0)
local offset = sf.Vector4f.new(0.0, 0.0, 0.5, 1.0)
```

Two-dimensional vectors support arithmetic:

```lua
local start = sf.Vector2f.new(64.0, 64.0)
local step = sf.Vector2f.new(16.0, 0.0)

local moved = start + step * 2.0   -- Vector2f(96, 64)
local isSame = moved == sf.Vector2f.new(96.0, 64.0) -- true
```

- **Operators:** 2D vectors support `+`, `-`, unary `-` and `==`, plus `*` and `/` with a scalar number. Ordering comparisons such as `<` are not defined, and vector-by-vector multiplication is a method rather than an operator.
- **Methods:** All three `Vector2` types provide `lengthSquared`, `perpendicular`, `dot`, `cross`, `componentWiseMul`, `componentWiseDiv` and `unpack` (returning the components as separate values). `sf.Vector2f` adds `length`, `normalized`, `angle`, `angleTo`, `rotatedBy` and `projectedOnto`, which work in `sf.Angle` rather than degrees.

`sf.Vector2f.new` also accepts a length and an `sf.Angle`, constructing the vector from polar coordinates:

```lua
local forward = sf.Vector2f.new(1.0, sf.degrees(90)) -- Vector2f(0, 1)
```

### Colour, rectangles and time

`sf.Color` holds `r`, `g`, `b` and `a` in the range 0–255. The four-argument constructor takes red, green, blue and alpha, and shorter forms use defaults for omitted components:

```lua
local solid = sf.Color.new(32, 96, 160)
local veil = sf.Color.new(0, 0, 0, 160)
```

Predefined colours are fields on `sf.Color`: `Black`, `White`, `Red`, `Green`, `Blue`, `Yellow`, `Cyan`, `Magenta` and `Transparent`. `toInteger` returns the packed 32-bit value. `sf.Color` defines no operators, so it cannot be added or compared directly; read and compare its components instead.

`sf.IntRect.new` and `sf.FloatRect.new` take a position and a size, or four numbers. The last two numbers are width and height, not the bottom-right corner:

```lua
local tile = sf.IntRect.new(sf.Vector2i.new(0, 0), sf.Vector2i.new(32, 32))
local same = sf.IntRect.new(0, 0, 32, 32)
local sub = sf.IntRect.new(32, 0, 32, 32)

local origin = tile.position      -- Vector2i(0, 0)
local height = tile.size.y        -- 32
local inside = tile.contains(sf.Vector2i.new(16, 16)) -- true
local overlap = tile.findIntersection(sub) -- an sf.IntRect, or nil, when the rectangles do not overlap
```

`sf.Angle` represents a rotation rather than a raw number, so pass values through `sf.degrees` or `sf.radians`:

```lua
local facing = sf.degrees(90)
local radians = sf.radians(math.pi / 2)

local degrees = facing.asDegrees()
local wrapped = sf.degrees(450).wrapSigned().asDegrees() -- 90
```

`wrapSigned` normalises to `[-180°, 180°)` and `wrapUnsigned` to `[0°, 360°)`; unwrapped angles are also valid. `sf.Angle.new()` and `sf.Angle.Zero` are both zero.

`sf.Time` represents a duration constructed with `sf.seconds`, `sf.milliseconds` or `sf.microseconds`:

```lua
local cooldown = sf.seconds(0.75)

local milliseconds = cooldown.asMilliseconds() -- 750
```

`sf.Time.new()` is zero, and `sf.sleep` pauses the calling thread for the given duration.

## Object types

Graphics objects mirror the SFML inheritance hierarchy in Lua:

```text
sf.Drawable
sf.Transformable
sf.Shape : sf.Drawable, sf.Transformable
sf.CircleShape, sf.ConvexShape, sf.RectangleShape : sf.Shape
sf.Sprite : sf.Drawable, sf.Transformable
sf.Text : sf.Drawable, sf.Transformable
```

Every `sf.Transformable` exposes `setPosition` and `getPosition` (`sf.Vector2f`), `setRotation` and `getRotation` (`sf.Angle`), `setScale` and `getScale`, `setOrigin` and `getOrigin` (`sf.Vector2f`), plus `move`, `rotate`, `scale` and `getTransform`.

```lua
local cell = sf.RectangleShape.new(sf.Vector2f.new(32.0, 32.0))

cell:setPosition(sf.Vector2f.new(64.0, 64.0))
cell:setOrigin(sf.Vector2f.new(16.0, 16.0))
cell:setRotation(sf.degrees(45))
cell:setScale(sf.Vector2f.new(2.0, 2.0))
cell:move(sf.Vector2f.new(8.0, 0.0))

local bounds = cell:getGlobalBounds()
```

`sf.RectangleShape.new` takes a size, `sf.CircleShape.new` takes a radius and a point count, `sf.ConvexShape.new` takes a point count and `sf.Text.new` takes a font, a string and a character size. Shapes also provide `setFillColor` and `setOutlineColor` (`sf.Color`), and `setOutlineThickness` (number).

The abstract base classes (`sf.Drawable`, `sf.Shape`, `sf.RenderTarget`, `sf.InputStream`, `sf.SoundSource`, `sf.SoundStream`, `sf.SoundRecorder`, `sf.SoundFileReader`, `sf.SoundFileWriter` and `sf.Socket`) exist as tables without `new` and cannot be constructed directly. Construct a concrete subclass instead and treat the base class as the parameter or return type.

`sf.Drawable` is the one base that Lua can implement directly: `sf.LuaDrawable.new(drawCallback)` creates a drawable whose `draw` runs in Lua.

Events follow this pattern in reverse: payload types such as `sf.Event_Closed` and `sf.Event_Resized` are constructible, while `sf.Event.new` requires one as its argument. Project code normally receives events rather than constructing them.

## Using SFML in a project

Positions, sizes and rectangles cross the Engine API as `sf` values, so converting between cell coordinates and world positions uses ordinary vector arithmetic:

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

Check the declared type before passing a value: an API expecting `sf.Vector2f` rejects `sf.Vector2i`, a Standard `tuple` or a plain table without converting between them.

JSON and metadata store pure data whereas `sf` values are native userdata, so conversion is explicit. The Game project's save codec writes an `sf.Vector2i` as `{ x, y }` and rebuilds it on load, and its record normaliser turns `sf.Vector2i`, `sf.Vector3i`, `sf.IntRect`, `sf.FloatRect` and `sf.Color` into numeric arrays. Store raw components for data destinations and reconstruct the `sf` value where it is used.

## Where to read more

`Scripts/stub/LuaSF.d.lua` provides the complete Lua declarations and SFML comments indexed by EmmyLua for completion and hover. Member names and parameter order closely follow the official SFML C++ reference linked above.
