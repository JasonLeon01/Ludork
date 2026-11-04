# Ludork Lua 进阶

Standard 在 `Scripts/Entry.lua` 运行前提供原生类、容器和工具函数，无需 `require`。完整签名见 `Scripts/stub/Standard.d.lua` 与 `Scripts/stub/Class.d.lua`；`Engine`、`GlobalCore`、`GlobalFunctions` 和 LuaSF 另有各自的绑定。

## Ludork 类

先写一张不带元表的普通定义表，加上默认值和方法，最后返回 `class(definition, ...)`。定义表不能已经定型。每个基类都必须是已定型的 Ludork 类或已注册的原生类型。重复的基类会被拒绝。基类顺序决定 C3 方法解析顺序，即查找继承成员时的顺序。

```lua
local Engine = require("Engine")

local Actor = Engine.Actor

local Pickup = {}

Pickup.count = 1
Pickup.sound = ""

function Pickup:init(texture, tag)
    Actor.init(self, texture, tag)
    self._collected = false
end

return class(Pickup, Actor)
```

### 默认值与实例状态

可编辑或可继承的默认值，都要在定型之前写到定义表上，包括 metadata 字段、组件默认值、`default*` spawn 值，以及允许蓝图覆写的标识符。

`init` 负责应用构造参数，并建立每个实例自己的状态。若每个实例都要持有一份独立的值，就复制类上的可变默认值：

```lua
local RouteFollower = {}

RouteFollower.speed = 96.0
RouteFollower.defaultRoute = {}

function RouteFollower:init()
    self._route = deepcopy(self.defaultRoute)
    self._routeIndex = 1
end

return class(RouteFollower)
```

不要把公开的默认值只写在 `init` 里。子类和蓝图都需要从类定义中读取这些默认值。

从蓝图、组件或通用数据还原出来的显式 `nil` 是一个自有字段，会覆盖继承来的默认值。继承、`copy` 与 `deepcopy` 都会保留这个覆盖；在 Lua 中赋值为 `nil` 会清除覆盖，重新按继承链查找。原生属性以及 Lua 的 getter、setter 行为不变。

### 构造、生命周期与继承

`Type.new(...)` 构造实例并调用 `init`。已定型的类也可以直接调用，但项目代码要沿用相邻模块的写法。没有 `init` 的类不接受参数，除非它唯一的原生根类型定义了构造函数契约。必需的原生基类初始化器要显式调用。`onCreate`、`onTick`、`onDestroy` 这类生命周期方法，只在所属运行时契约调用它们时才执行。`instance:dispose()` 至多执行一次解析出的实现，释放原生所有权与实例字段，垃圾回收不会再调用它。

原生根对象尚未构造时，对其属性的类型化赋值先存入复合实例，并在原生基类初始化时应用。原生属性通过对应根对象访问；脚本 getter 与 setter 从脚本类中解析。setter 错误会向调用方传播。

在方法内部，`super()` 从定义该方法的类开始解析下一个实现。间接回调让归属变得不明确时，改用 `super(ClassName, self)`。Ludork 支持多重继承，但声明的基类顺序必须保持稳定。

[脚本 Mixin](<../Lua 与蓝图脚本/脚本 Mixin/运行时契约.md>)使用生命周期回调，不能声明 `init`。

### Class 工具

| API | 用途 |
|---|---|
| `Class.isInstance(value, target)` | 用类的完整 MRO 判断原始 Lua 类型或实例。 |
| `Class.isSubclass(value, target)` | 按完整 MRO 判断它是否为子类。 |
| `Class.type(value)` | 返回它所属的 Ludork 类，或原生回退类型。 |
| `Class.hasOwnField(value, key)` | 判断字段是否直接存储在该值上。 |
| `Class.getMro(value)` | 返回一份独立的 MRO 数组。 |
| `Class.getParameterNames(callable)` | 返回声明的参数名，不含 `self`。 |
| `Class.constructNamed(type, values)` | 把具名值对应到 `init` 参数上并构造实例。 |
| `Class.super(cls?, self?)` / `super(...)` | 在当前生效的 MRO 中解析下一个实现。 |
| `Class.monitor` / `Class.unmonitor` | 为字段添加或移除具名订阅。 |
| `Class.MISSING` | 被监听字段此前没有值时使用的哨兵值。 |

`Class.monitor(target, name, callback, params?, notifyEqualWrites?, identifier?)` 订阅 table 或 userdata 上的字段，注册时不会立刻调用回调。`identifier` 默认为 `""`：标识相同就在原位替换订阅，标识不同则按注册顺序依次执行。`Class.unmonitor(target, name, identifier?)` 只移除对应的那条订阅，订阅不存在时不做处理。

被监听的字段拒绝 `nil` 赋值，默认只在可见值发生变化时通知；该订阅把 `notifyEqualWrites` 传为 `true` 后，等值写入也会收到通知。旧值缺失或是显式 `nil` 时，回调收到 `Class.MISSING`。每个回调收到的参数是 `(oldValue, newValue, ...)`，后面接着 `params` 中的各项。回调中再次赋值仍然生效，但每个订阅会抑制自身的递归。回调报错会终止本次分发并向外传播，不会回滚赋值。

分发使用订阅快照。轮到某条订阅之前它已被移除或替换，就跳过它的旧回调；新注册的回调从后续赋值开始生效。最后一条订阅被移除后，恢复普通字段存储与 table 的原始元表。

### 类型判断

所有类型判断都用 `Class.isInstance`。它的 target 可以是已定型的 Ludork 类、已注册的原生类，也可以是精确的 Lua 类型名：`nil`、`boolean`、`number`、`string`、`function`、`userdata`、`thread`、`table`。

```lua
if Class.isInstance(value, "string") then
    print(string.upper(value))
end

local isActor = Class.isInstance(actor, Actor)
local isNativeVector = Class.isInstance(position, sf.Vector2f)
local isInteger = Class.isInstance(value, "number") and math.type(value) == "integer"
```

target 用字符串时，判断保持 Lua `type` 的语义：原生容器是 userdata，可调用的 table 仍然是 table。未知的类型名返回 false。target 既不是字符串也不是 table 时，属于参数错误。`math.type` 只在确认值为 number 之后使用，`Class.type` 只在确实需要解析后的类型值时使用。不要用 `==` 或 `~=` 比较 Lua `type` 的结果。

## 项目真值与复制

Ludork 的 `bool(value)` 扩展了 [Lua 真值规则](<Lua 基础语法.md#运算符>)：`0`、空字符串、空 table 与空原生容器也为假。“缺失”是独立协议值时，才显式比较 `nil`。

| API | 行为 |
|---|---|
| `copy(value)` | 浅复制 table 以及支持该操作的原生值。 |
| `deepcopy(value)` | 递归复制键与值，同时保留别名与循环结构。 |
| `asizeof(value)` | 估算一份 Lua 或原生值图占用的内存字节数。 |

已知的绑定值类型直接使用 `value:copy()` 或 `value:deepcopy()`，例如 `position:copy()`、`rect:copy()` 和 `colour:copy()`，得到可独立修改的值。全局 `copy` 与 `deepcopy` 用于 Lua table 和动态数据结构；遇到支持复制的绑定值时，调用同一套 LuaGlue 复制入口。不支持值复制的资源仍引用原对象。

被监听的 table，`copy` 与 `deepcopy` 会复制当前的逻辑字段值并保留原始元表，不带监听代理与订阅。复制后的值可以正常保存。订阅属于运行中的实例，读档后需要重新注册。

## 原生容器

`list`、`tuple` 与 `dict` 是原生容器。它们不能取代 JSON、metadata、蓝图值或 Core 绑定所要求的 Lua table。

```lua
local route = list("north", nil, "east")
local position = tuple(4, 7)
local visited = dict {
    [position] = true,
}
```

`list(...)` 与 `tuple(...)` 把参数存进从 1 开始的槽位。只传一个 table、list 或 tuple 参数时，会按序列浅复制它。`dict(mapping?)` 复制传入的映射。tuple 拒绝 `nil`，list 与 dict 则保留 `nil` 槽位或 `nil` 值。序列的负索引无效，对 list 的写入或插入只能指向已有槽位或 `#value + 1`。

| 类型 | 主要操作 |
|---|---|
| `list` | `append`、`extend`、`insert`、`pop`、`remove`、`clear`、`index`、`count`、`contains`、`reverse`、`sort`、`copy`、`unpack`、`toTable` |
| `tuple` | `index`、`count`、`contains`、`unpack`、`toTable` |
| `dict` | `get`、`setdefault`、`update`、`pop`、`remove`、`clear`、`contains`、`keys`、`values`、`items`、`copy`、`toTable` |

原生 `ipairs` 会访问 list 中的 `nil` 槽位，dict 迭代保持插入顺序。迭代期间修改结构会失败。`dict:get(key, default)` 只在键不存在时使用 default，`dict:contains(key)` 则能区分“键不存在”与“存了 `nil` 值”。删除条目用 `remove` 或 `pop`，不要写 `dictionary[key] = nil`。`remove` 返回是否找到了该键；`pop` 在键不存在且未给出 default 时报错。

`list:pop()` 默认删掉最后一个槽位，`list:remove` 在值不存在时报错，`list:sort(compare?)` 是稳定排序。dict 的 `keys` 与 `values` 返回按插入顺序排列的 list 快照，`items` 是惰性迭代器。直接查找时，dict 的方法名优先于同名的 string 键，因此这类条目要改用 `value:get("get")` 或 `dict.get(value, "get")` 读取。

list 与 dict 按结构比较，带环的图也一样。tuple 不可变，可作原生 dict 的键：boolean、有限 number、string 与嵌套 tuple 按值比较，可变值或引用值按身份比较。`nil` 与非有限 number 不能作为 tuple 键。普通 Lua table 仍然按身份为 tuple userdata 计算哈希。

### 转换边界

`toTable` 递归转换原生容器，同时保留别名与循环。转换出的 list 与 tuple 即使为空或被嵌套，也保持 JSON 数组的形状。list 的 `nil` 槽位与 dict 的 `nil` 值会变成 `cjson.null`。tuple 键会转成 `(1,2)` 这样的紧凑字符串，与已有的 string 键撞名时失败。JSON 编码器、metadata、蓝图值与 Core 绑定都不会自动完成这种转换。

如果加载之后必须重建键的身份，就改用 record array。

## 数学扩展

Standard 为 Lua 的 `math` 库补充下列函数。除 `isFinite` 外，参数都必须是有限的 Lua 数字，数字字符串要先显式调用 `tonumber` 转换。非法区间、算术溢出，以及整数结果超出 Lua 整数范围，都会报错。`gcd` 与 `lcm` 要求 Lua 整数。`round` 与 `trunc` 原样保留整数输入。

`lerp`、`inverseLerp` 与 `remap` 支持反向区间与外推，不做夹紧。

| API | 行为 |
|---|---|
| `math.isFinite(value)` | 有限 Lua 数字返回 true，否则返回 false。 |
| `math.clamp(value, min, max)` | 返回夹在 `[min, max]` 内的浮点数；要求 `min <= max`。 |
| `math.lerp(a, b, t)` | 按比例 `t` 从 `a` 线性插值到 `b`。 |
| `math.round(value)` | 舍入到最近的整数，正好在中点时取偶数。 |
| `math.trunc(value)` | 向零截断并返回整数，与 `math.tointeger` 的精确转换不同。 |
| `math.isNearZero(value, epsilon=0.1)` | 判断 `abs(value) < epsilon`；要求 `epsilon >= 0`。 |
| `math.gcd(a, b)` | 返回非负的最大公约数；`gcd(0, 0)` 为 0。 |
| `math.lcm(a, b)` | 返回非负的最小公倍数；任一参数为 0 时返回 0。 |
| `math.sign(value)` | 返回整数 −1、0 或 1；正零与负零都返回 0。 |
| `math.inverseLerp(a, b, value)` | 返回 `(value - a) / (b - a)`；要求 `a ~= b`。 |
| `math.remap(value, inMin, inMax, outMin, outMax)` | 按比例把输入区间映射到输出区间；要求 `inMin ~= inMax`。 |
| `math.smoothstep(edge0, edge1, value)` | 把归一化位置 `t` 夹到 `[0, 1]`，再返回 `t²(3 − 2t)`；要求 `edge0 < edge1`。 |
| `math.moveTowards(current, target, maxDelta)` | 最多移动 `maxDelta`，且不越过目标；要求 `maxDelta >= 0`。 |

## string 与 table 扩展

Standard 在 Lua 自带库的基础上补充了下列辅助函数：

| API | 用途 |
|---|---|
| `string.pformat` | 用 `{}` 按位置填充字段，或用末尾的映射表填充 `{name}` 字段；`{{` 与 `}}` 用于转义花括号。 |
| `string.contains`、`string.startsWith`、`string.endsWith` | 判断文本中是否包含某段内容，或以某段内容开头、结尾。 |
| `string.isEmpty`、`string.isBlank` | 判断文本为空，还是只含空白字符。 |
| `string.strip`、`string.stripLeading`、`string.stripTrailing` | 移除 Unicode 空白字符。 |
| `string.replace`、`string.split` | 替换文本，或按分隔符拆分文本。 |
| `string.utf8Length`、`string.utf8Slice` | 按 UTF-8 码点计算长度或切片。 |
| `string.graphemeLength` | 统计 Unicode 字素簇（用户感知的字符）数量。 |
| `string.stripUnicode` | 去掉两端的 Unicode White_Space。 |
| `table.contains`、`table.index` | 用 Lua 相等语义在稠密 Lua 数组中查找，遇到第一个 `nil` 槽位即停止。 |
| `table.orderedStringKeys` | 先返回优先键（preferred keys），再返回其余按序排列的 string 键。 |

## 平台、配置与文件

`PLATFORM` 是目标平台的小写名称（`win32`、`darwin`、`ios`、`android`、`ohos`，或 CMake 系统名）。`LUDORK_MOBILE` 与 `LUDORK_DESKTOP` 给出运行时宿主类别。不要从路径或环境变量推断平台。

`configparser.ConfigParser()` 提供 `read`、`write`、`has_section`、`add_section`、`get`、`getfloat`、`getint`、`getboolean` 与 `set`。`read(path)` 和 `write(path)` 在扩展名为 `.plist` 时选择 XML plist，扩展名不区分大小写；该后端仅在 macOS 上可用。其他路径使用 INI，因此 macOS 仍可显式读写 `.ini` 文件。默认游戏配置应通过[游戏配置路径 API](<../Lua 与蓝图脚本/Game 玩法/运行时数据、配置与存档.md#配置>)获取。

配置 plist 包含一个根字典，根字典的键是配置节名称。每个配置节又是一个字典，其中的设置值全部为字符串，包括以文本保存的数值和布尔值。例如：

```xml
<plist version="1.0">
  <dict>
    <key>Main</key>
    <dict>
      <key>scale</key><string>1.25</string>
      <key>verticalsync</key><string>true</string>
    </dict>
  </dict>
</plist>
```

不支持二进制 plist，也不支持 `<integer>`、`<real>`、`<true/>`、数组或嵌套字典等非字符串设置值。空配置节、空字符串、Unicode 文本和值内部的空白都会保留，写入时会转义 `&`、`<` 等 XML 字符。同一 plist 配置节中，只有大小写不同的重复键会被拒绝。

运行时解析器的配置节名称区分大小写，设置键不区分大小写，并保留最近一次 `set` 提供的键名拼写。所有值以字符串保存，`getfloat`、`getint` 与 `getboolean` 在读取时转换。布尔文本接受 `1`、`yes`、`true`、`on` 和 `0`、`no`、`false`、`off`，忽略大小写及两端空白。键缺失或转换失败时，返回传入的回退值；未传回退值则返回 `nil`。多次成功读取会合并到现有配置节中，并保留无关键。

文件不存在时，`read` 返回 `false`；INI 读取器在无法打开文件时也返回 `false`。已有 plist 无法读取、结构无效或使用不支持的格式时会报错。plist 解析失败不会改变解析器已有的配置节。`write` 先完成序列化，再打开目标文件，因此序列化失败不会改动已有文件；文件写入错误也会报错。

`locale.getdefaultlocale()` 返回宿主 locale 对。

文件系统扩展如下：

| API | 用途 |
|---|---|
| `os.getcwd`、`os.listdir` | 读取当前目录，或列出其中已排序的直接条目名（相对于该目录）。 |
| `os.createDirectories` | 创建 UTF-8 目录路径及其缺失的父目录；目录已存在也视为成功。 |
| `os.removeFile` | 删除一个 UTF-8 文件路径；文件不存在或操作失败都会报错。它不取代 `os.remove`。 |
| `os.path.join`、`os.path.splitext`、`os.path.basename`、`os.path.dirname`、`os.path.abspath` | 拼接或解析路径。 |
| `os.path.isdir`、`os.path.isfile` | 判断路径是否为已存在的目录或文件；路径不存在时返回 false，检查过程中的错误照常抛出。 |
| `os.path.getmtime` | 返回文件的修改时间戳。 |

`os.listdir`、`os.path.isdir`、`os.path.isfile` 与 `io.open` 只使用真实文件系统，`/Game/Assets/...` 只能交给 Ludork 资源 API。对于已打包的 Data，`os.path.getmtime("Data/...")` 返回所在 `.ldpak` 文件的修改时间戳；其他文件系统 API 不暴露归档条目。

Standard 还会安装以下二进制安全的编解码全局对象：

| API | 用途 |
|---|---|
| `zlib.compress`、`zlib.decompress` | 把数据压缩成 zlib 格式，或解压 zlib、gzip 数据。 |
| `base64.encode`、`base64.decode` | 编码或解码 Base64；解码接受空白与合法 padding，并拒绝畸形数据。 |

## 协作任务与文件批次

`asyncio.create_task(callback, ...)` 启动协作式任务。在任务中，用 `asyncio.sleep(seconds)` 延时，或用 `asyncio.await(operation)` 等待异步结果，都不会阻塞游戏循环。两者只能在 `asyncio.create_task` 创建的任务内使用。

例如，在活动的 `SceneMap` 中，按顺序播放以下已配置的教程：

```lua
local Scene = require("GlobalFunctions.Scene")

local task = asyncio.create_task(function ()
    asyncio.await(Scene.ShowTutorial("Map01Menu"))
    asyncio.await(Scene.ShowTutorial("Map01Dialogue"))
    asyncio.await(Scene.ShowMessage("向导", "可以继续了。"))
end)
```

`await` 返回操作的最终结果；操作被取消时，任务结束，不执行下一句。若只需停止这个任务，调用 `asyncio.cancel_task(task)`，共享操作及其他等待者仍继续运行。任务完成或取消时会执行 Lua `__close` 清理。

操作 API 见 [AsyncOperation](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/事件与模块根.md#asyncoperation>)，自定义入口的初始化见 [Lua 运行时与模块](<../Lua 与蓝图脚本/Lua 运行时与模块.md#异步操作更新>)。

读取量较大时，用 `asyncio.start_file_batch(specs)` 处理：最多并发扫描并读取四个文件，返回结果时仍保持输入清单的顺序。逻辑 `Data/...` 根路径支持已打包的 Data，其余根路径使用物理文件系统。`asyncio.poll_file_batch(job, maxItems?)` 返回进度、已交付条目与结构化错误，`asyncio.cancel_file_batch(job)` 取消整个任务。

`poll_file_batch` 返回的快照带有字符串 `state`。比较时使用 `asyncio` 上的常量：`FILE_BATCH_SCANNING = "scanning"`、`FILE_BATCH_RUNNING = "running"`、`FILE_BATCH_COMPLETED = "completed"`、`FILE_BATCH_CANCELLING = "cancelling"`、`FILE_BATCH_CANCELLED = "cancelled"`、`FILE_BATCH_FAILED = "failed"`。completed、cancelled、failed 是批次任务的终态；已经单独交付的数据转换仍按下文的分步转换契约处理。

开启 `parseData = true` 后，后缀必须以 `.json` 或 `.ldc` 结尾；后台线程解码 JSON 或二进制 LDC，轮询返回 `FileBatchDataConversion`。解码失败时 `operation = "parse"`。在 Lua 逻辑线程上用 `asyncio.step_file_batch_data(conversion, maxNodes, maxMilliseconds)` 推进转换，两个预算值都要为正。它返回 `completed, processed, data`，data 在完成时交付一次。数组用 `n` 记录逻辑长度；根值为 null 时返回 nil，数组 null 槽位不保留键，对象 null 字段保留为 `cjson.null`，与 `Engine.loadData` 一致。`contentBytes` 表示解包后、转换为 Lua 值前的载荷字节数。`asyncio.clear_file_batch_data` 可重复调用以释放转换资源，取消、回收和运行时关闭 也会让它失效。转换不能跨 Lua VM 或线程使用。

不开启 `parseData` 时，`item.content` 保留原始文件字节，包括 LDC 文件头与编码后的载荷；需要数据时交给 `Engine.decodeData` 解码。蓝图缓存保留这些字节以延迟解码。

## 计时与内存诊断

`perfCounter()` 返回单调递增的高精度时间，单位为秒。`processMemoryMB()` 返回当前进程占用的内存，单位为 MiB。估算某个值及其关联对象占用的内存时，使用 `asizeof`。

## 相关页面

- [SFML 快速入门](<SFML 快速入门.md>)
