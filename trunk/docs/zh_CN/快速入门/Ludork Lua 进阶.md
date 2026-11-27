# Ludork Lua 进阶

Ludork 的 Standard 库会在 `Scripts/Entry.lua` 运行前自动加载原生类、容器、数学与字符串工具函数、配置解析器以及协作式异步 API，无需调用 `require`。其声明位于 `Scripts/stub/Standard.d.lua` 与 `Scripts/stub/Class.d.lua`；`Engine`、`GlobalCore`、`GlobalFunctions` 与 LuaSF 则各自拥有独立的绑定。

## Ludork 类

先创建一张不含元表的普通定义表，添加默认值与方法后，返回 `class(definition, ...)`。定义表不得处于已定型状态，每个基类都必须是已定型的 Ludork 类或已注册的原生类型，且不允许传入重复基类。基类顺序决定用于查找继承成员的 C3 方法解析顺序（MRO）。

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

所有可编辑或可继承的默认值均应在定型前写入定义表，包括元数据字段、组件默认值、`default*` 生成默认值以及允许蓝图覆写的标识符。

`init` 负责应用构造参数并初始化各实例的独立状态。当每个实例需要持有独立副本时，应复制类上的可变默认值：

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

> **重要**：不要仅在 `init` 中初始化公开默认值。子类与蓝图发现机制均从类定义表中读取默认值。

从蓝图、组件或通用数据还原的显式 `nil` 会作为自有字段存储并覆盖继承默认值；继承、`copy` 与 `deepcopy` 均会保留该覆盖。在 Lua 中赋值为 `nil` 会清除该覆盖并恢复按继承链查找。原生属性及 Lua 的 getter/setter 遵循各自的常规查找与分发规则。

### 构造、生命周期与继承

- **构造**：调用 `Type.new(...)` 构造实例并执行 `init`。已定型的类本身也可直接调用，编写代码时应与相邻模块保持风格一致。未定义 `init` 的类不接受参数，除非其唯一的原生根类型定义了构造函数契约。必需的原生基类初始化器必须显式调用。
- **原生属性延迟应用**：在原生根对象尚未构造完成前，对其属性的类型化赋值会先暂存于复合实例上，并在原生基类初始化时一并应用。原生属性通过该根对象访问；脚本 getter 与 setter 从脚本类解析，setter 抛出的错误会传播给调用方。
- **生命周期与销毁**：`onCreate`、`onTick`、`onDestroy` 等生命周期方法仅在其所属运行时契约调用时执行。`instance:dispose()` 至多执行一次解析到的销毁实现，释放原生所有权与实例字段，且不会在垃圾回收时重复触发。
- **super 调用与多重继承**：在方法内部，调用 `super()` 会从定义该方法的类开始解析下一个实现；若间接回调导致归属不明确，请改用 `super(ClassName, self)`。系统支持多重继承，但声明的基类顺序必须保持稳定。
- **Script Mixin**：[Script Mixin](<../Lua 与蓝图脚本/脚本 Mixin/运行时契约.md>)使用生命周期回调，而不使用 `init`。

### Class 工具

| API | 用途 |
|---|---|
| `Class.isInstance(value, target)` | 按类的完整 MRO 判断原始 Lua 类型或实例。 |
| `Class.isSubclass(value, target)` | 按完整 MRO 判断是否为子类。 |
| `Class.type(value)` | 返回所属 Ludork 类或原生回退类型。 |
| `Class.hasOwnField(value, key)` | 判断字段是否直接存储在目标对象上。 |
| `Class.getMro(value)` | 返回一份独立的 MRO 数组。 |
| `Class.getParameterNames(callable)` | 返回声明的参数名称列表（不含 `self`）。 |
| `Class.constructNamed(type, values)` | 将具名值映射到 `init` 参数并构造实例。 |
| `Class.super(cls?, self?)` / `super(...)` | 在当前生效的 MRO 中解析下一个实现。 |
| `Class.monitor` / `Class.unmonitor` | 为字段添加或移除具名监听订阅。 |
| `Class.MISSING` | 被监听字段此前不存在值时传入的哨兵值。 |

调用 `Class.monitor(target, name, callback, params?, notifyEqualWrites?, identifier?)` 可订阅 table 或 userdata 字段的写入操作（注册时不会立即触发 `callback`），调用 `Class.unmonitor(target, name, identifier?)` 可移除对应订阅：

- **标识与顺序**：`identifier` 默认为 `""`。使用相同标识重复注册会在原位替换该订阅；不同标识的订阅按注册顺序依次执行。`Class.unmonitor` 仅移除指定标识的订阅，忽略不存在的标识。
- **值过滤与参数**：被监听字段拒绝 `nil` 赋值；除非 `notifyEqualWrites` 为 `true`，否则仅在可见值发生变化时触发通知。字段此前缺失或为显式 `nil` 时，旧值传入 `Class.MISSING`。回调按 `(oldValue, newValue, ...)` 接收参数，末尾附带 `params` 中的各项。
- **重入与错误处理**：回调内的重入赋值照常生效，但每条订阅会抑制自身的递归触发。回调抛出错误会中止本次分发并向外传播，不会回滚已完成赋值的字段。
- **快照分发与卸载**：分发过程遍历订阅快照：若某条订阅在轮到其执行前已被移除或替换，则跳过旧回调；新注册的回调从后续赋值开始生效。移除最后一条订阅后，字段恢复普通存储并还原 table 的原始元表。

### 类型判断

所有类型检查统一使用 `Class.isInstance`。其 `target` 参数可以是已定型的 Ludork 类、已注册的原生类，或精确的 Lua 类型名：`nil`、`boolean`、`number`、`string`、`function`、`userdata`、`thread`、`table`。

```lua
if Class.isInstance(value, "string") then
    print(string.upper(value))
end

local isActor = Class.isInstance(actor, Actor)
local isNativeVector = Class.isInstance(position, sf.Vector2f)
local isInteger = Class.isInstance(value, "number") and math.type(value) == "integer"
```

传入字符串目标时，判断遵循 Lua `type` 语义：原生容器属于 `userdata`，可调用 table 仍属于 `table`。未知名称返回 `false`；既非字符串也非 table 的目标会抛出参数错误。仅在确认值为 `number` 后再调用 `math.type`，仅在确实需要解析后的类型值时调用 `Class.type`，且不要对 Lua `type` 的返回值使用 `==` 或 `~=` 进行比较。

## 项目真值与复制

Ludork 的 `bool(value)` 扩展了 [Lua 真值规则](<Lua 基础语法.md#运算符>)：`0`、空字符串、空 table 以及空原生容器同样视为 `false`。若“值缺失”属于独立协议语义，应显式与 `nil` 比较。

| API | 行为 |
|---|---|
| `copy(value)` | 浅复制 table 及支持值复制的原生值。 |
| `deepcopy(value)` | 递归复制键与值，同时保留别名与循环引用。 |
| `asizeof(value)` | 估算 Lua 或原生值图占用的内存字节数。 |

对于已知的绑定值类型，可直接调用 `value:copy()` 或 `value:deepcopy()`（例如 `position:copy()`、`rect:copy()` 或 `colour:copy()`）获取独立副本。全局 `copy` 与 `deepcopy` 处理 Lua table 与动态值图，遇到支持的绑定值时会转调同一套 LuaGlue 复制入口。不支持值复制的资源按引用返回，不会复制。

对于被监听的 table，`copy` 与 `deepcopy` 仅复制当前的逻辑字段值并保留原始元表，不包含监听代理与订阅。复制结果可正常保存；监听订阅仅属于存活实例，读档后需重新注册。

## 原生容器

`list`、`tuple` 与 `dict` 是原生容器，不能替代 JSON、元数据、蓝图值或 Core 绑定所要求的 Lua table。

```lua
local route = list("north", nil, "east")
local position = tuple(4, 7)
local visited = dict {
    [position] = true,
}
```

| 类型 | 主要操作 |
|---|---|
| `list` | `append`、`extend`、`insert`、`pop`、`remove`、`clear`、`index`、`count`、`contains`、`reverse`、`sort`、`copy`、`unpack`、`toTable` |
| `tuple` | `index`、`count`、`contains`、`unpack`、`toTable` |
| `dict` | `get`、`setdefault`、`update`、`pop`、`remove`、`clear`、`contains`、`keys`、`values`、`items`、`copy`、`toTable` |

- **构造与索引**：`list(...)` 与 `tuple(...)` 将参数存入从 1 开始的槽位；仅传入单个 table、`list` 或 `tuple` 时，会按序列浅复制其内容。`dict(mapping?)` 复制传入的映射。`tuple` 拒绝 `nil`，而 `list` 与 `dict` 保留 `nil` 槽位与 `nil` 值。序列不支持负索引，`list` 的写入与插入只能指向已有槽位或 `#value + 1`。
- **遍历与查找**：原生 `ipairs` 会访问 `list` 中的 `nil` 槽位；`dict` 遍历保持插入顺序；遍历期间修改容器结构会抛出错误。`dict:get(key, default)` 仅在键不存在时返回 `default`，`dict:contains(key)` 可区分键缺失与存储的 `nil` 值。在 `dict` 上直接查找时，方法名优先于同名字符串键，读取此类键应改用 `value:get("get")` 或 `dict.get(value, "get")`。
- **修改与快照**：删除字典条目应使用 `remove` 或 `pop`，而非 `dictionary[key] = nil`。`remove` 返回是否找到该键；`pop` 在键缺失且未提供默认值时抛出错误。`list:pop()` 默认移除最后一个槽位，`list:remove` 在目标值不存在时抛出错误，`list:sort(compare?)` 执行稳定排序。`dict:keys()` 与 `dict:values()` 返回按插入顺序排列的 `list` 快照，`dict:items()` 返回惰性迭代器。
- **相等比较与 tuple 键**：`list` 与 `dict` 按结构比较，支持带环的值图。`tuple` 不可变，可用作原生字典的键：布尔值、有限数字、字符串及嵌套 `tuple` 按值比较，可变值或引用值按对象身份比较。`nil` 与非有限数字不可作为 `tuple` 键。普通 Lua table 对 `tuple` userdata 仍按对象身份计算哈希。

### 转换边界

`toTable` 会递归转换原生容器并保留别名与循环引用。转换后的 `list` 与 `tuple` 在为空或嵌套时均保持 JSON 数组形态；`list` 的 `nil` 槽位与 `dict` 的 `nil` 值转为 `cjson.null`；`tuple` 键转为形如 `(1,2)` 的紧凑字符串，若与已有字符串键冲突则抛出错误。JSON 编码器、元数据、蓝图值与 Core 绑定均不会自动执行此转换。

若加载后必须还原键的原始身份，请改用记录数组。

## 数学扩展

Standard 为 Lua 的 `math` 库扩展了以下函数。除 `isFinite` 外，所有参数均须为有限 Lua 数字（数字字符串需先显式调用 `tonumber` 转换）。无效区间、算术溢出以及超出 Lua 整数范围的整数结果均会抛出错误。`gcd` 与 `lcm` 要求 Lua 整数参数；`round` 与 `trunc` 会原样保留整数输入。

`lerp`、`inverseLerp` 与 `remap` 支持反向区间及外推，不进行范围限制。

| API | 行为 |
|---|---|
| `math.isFinite(value)` | 若为有限 Lua 数字返回 `true`，否则返回 `false`。 |
| `math.clamp(value, min, max)` | 返回限制在 `[min, max]` 区间内的浮点数；要求 `min <= max`。 |
| `math.lerp(a, b, t)` | 按比例 `t` 从 `a` 线性插值至 `b`。 |
| `math.round(value)` | 四舍五入至最近整数，恰好位于中点时取偶数。 |
| `math.trunc(value)` | 向零截断并返回整数，区别于 `math.tointeger` 的精确转换。 |
| `math.isNearZero(value, epsilon=0.1)` | 判断 `abs(value) < epsilon`；要求 `epsilon >= 0`。 |
| `math.gcd(a, b)` | 返回非负最大公约数；`gcd(0, 0)` 为 `0`。 |
| `math.lcm(a, b)` | 返回非负最小公倍数；任一参数为 `0` 时返回 `0`。 |
| `math.sign(value)` | 返回整数 `-1`、`0` 或 `1`；正零与负零均返回 `0`。 |
| `math.inverseLerp(a, b, value)` | 返回 `(value - a) / (b - a)`；要求 `a ~= b`。 |
| `math.remap(value, inMin, inMax, outMin, outMax)` | 将输入区间按比例映射到输出区间；要求 `inMin ~= inMax`。 |
| `math.smoothstep(edge0, edge1, value)` | 将归一化位置 `t` 限制在 `[0, 1]`，再返回 `t²(3 − 2t)`；要求 `edge0 < edge1`。 |
| `math.moveTowards(current, target, maxDelta)` | 向目标最多移动 `maxDelta` 且不越过目标；要求 `maxDelta >= 0`。 |

## string 与 table 扩展

Standard 在 Lua 标准库基础上扩展了以下辅助函数：

| API | 用途 |
|---|---|
| `string.pformat` | 使用 `{}` 填充位置参数，或从末尾映射表填充 `{name}` 具名字段；`{{` 与 `}}` 用于转义花括号。 |
| `string.contains`、`string.startsWith`、`string.endsWith` | 判断文本是否包含指定内容，或检查前缀与后缀。 |
| `string.isEmpty`、`string.isBlank` | 判断文本为空，或仅包含空白字符。 |
| `string.strip`、`string.stripLeading`、`string.stripTrailing` | 移除 Unicode 空白字符。 |
| `string.replace`、`string.split` | 替换或拆分文本。 |
| `string.utf8Length`、`string.utf8Slice` | 按 UTF-8 码点计算长度或切片。 |
| `string.graphemeLength` | 统计 Unicode 字素簇（用户感知字符）数量。 |
| `string.stripUnicode` | 移除两端的 Unicode `White_Space` 字符。 |
| `table.contains`、`table.index` | 按 Lua 相等语义在连续 Lua 数组中查找，遇到首个 `nil` 槽位即停止。 |
| `table.orderedStringKeys` | 优先返回指定键，随后返回按顺序排列的其余字符串键。 |

## 平台、配置与文件

`PLATFORM` 为目标平台的小写名称（`win32`、`darwin`、`ios`、`android`、`ohos`、`web` 或 CMake 系统名）。`LUDORK_MOBILE` 与 `LUDORK_DESKTOP` 暴露运行时宿主类别：在 HarmonyOS 上，手机与平板报告为移动端，二合一设备报告为桌面端。请勿通过路径或环境变量推断运行平台。

`configparser.ConfigParser()` 提供 `read`、`write`、`has_section`、`add_section`、`get`、`getfloat`、`getint`、`getboolean` 与 `set`。在 macOS 上，`read(path)` 与 `write(path)` 对 `.plist` 扩展名（不区分大小写）选用 XML plist 格式；所有其他路径（包括 macOS 上显式指定的 `.ini` 文件）均使用 INI 格式。获取当前平台的默认游戏配置文件路径请参阅[游戏配置路径 API](<../Lua 与蓝图脚本/默认玩法/运行时数据、配置与存档.md#配置>)。

配置 plist 包含一个根字典，其键为配置节名称，每个配置节均为包含键值对的字典，所有设置值均以字符串保存（包括文本形式的数值与布尔值）：

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

- **Plist 限制**：不支持二进制 plist 及非字符串值（`<integer>`、`<real>`、`<true/>`、数组或嵌套字典），且同一配置节内拒绝仅大小写不同的重复键。空配置节、空字符串、Unicode 文本与值内部空白均会保留；写入时自动转义 `&`、`<` 等 XML 字符。
- **大小写敏感性与类型转换**：配置节名称区分大小写；键名不区分大小写，并保留最后一次调用 `set` 时传入的拼写。所有值均按字符串存储，由 `getfloat`、`getint` 与 `getboolean` 在读取时转换（布尔值将 `1`/`yes`/`true`/`on` 解析为 `true`，将 `0`/`no`/`false`/`off` 解析为 `false`，忽略大小写及两端空白）。键缺失或转换失败时返回回退值或 `nil`。
- **合并与读取错误**：多次成功读取会合并至已有配置节，并保留无关的键。文件不存在时 `read` 返回 `false`（INI 在无法打开文件时同样返回 `false`）；若已存在的 plist 无法读取、格式畸形或包含不支持的结构，则会抛出错误，且不修改现有配置节。
- **写入原子性**：`write` 先在内存中完成序列化再打开目标文件，因此序列化失败不会破坏已有文件；文件写入错误会抛出异常。

`locale.getdefaultlocale()` 返回宿主的区域设置对。

文件系统扩展如下：

| API | 用途 |
|---|---|
| `os.getcwd`、`os.listdir` | 获取当前工作目录，或列出目录下已排序的直接条目名称（相对路径）。 |
| `os.createDirectories` | 创建 UTF-8 目录路径及所有缺失的父目录；目录已存在时视作成功。 |
| `os.removeFile` | 删除指定 UTF-8 路径的文件；文件缺失或删除失败均抛出错误。不替代 `os.remove`。 |
| `os.path.join`、`os.path.splitext`、`os.path.basename`、`os.path.dirname`、`os.path.abspath` | 拼接或解析文件路径。 |
| `os.path.isdir`、`os.path.isfile` | 判断路径是否为已存在的目录或文件；路径缺失返回 `false`，检查出错则向外传播。 |
| `os.path.getmtime` | 返回文件的修改时间戳。 |

`os.listdir`、`os.path.isdir`、`os.path.isfile` 与 `io.open` 仅访问真实文件系统；`/Game/Assets/...` 路径仅适用于 Ludork 资源 API。对于已归档的 Data，`os.path.getmtime("Data/...")` 返回其所在 `.ldpak` 文件的时间戳，其他文件系统 API 不暴露归档内部条目。

二进制安全编解码全局对象如下：

| API | 用途 |
|---|---|
| `zlib.compress`、`zlib.decompress` | 将数据压缩为 zlib 格式，或解压 zlib / gzip 数据。 |
| `base64.encode`、`base64.decode` | 编解码 Base64；解码接受空白字符与合法填充，拒绝畸形数据。 |

## 协作任务与文件批次

`asyncio.create_task(callback, ...)` 调度协作式任务并返回任务句柄。在任务内部，可调用 `asyncio.sleep(seconds)` 延时，或调用 `asyncio.await(operation)` 等待异步结果，同时不阻塞游戏主循环。若任务发生 Lua 运行时错误，引擎会终止该任务、记录 Lua 调用栈，并继续执行其他任务。

对于必须成功才能继续运行游戏的关键流程，应使用 `asyncio.create_required_task(callback, ...)`。其参数与返回值句柄类型相同，但未处理的 Lua 错误会直接结束游戏会话。

例如，在活动的 `SceneMap` 中按顺序播放已配置的教程：

```lua
local Scene = require("GlobalFunctions.Scene")

local task = asyncio.create_task(function ()
    asyncio.await(Scene.ShowTutorial("Map01Menu"))
    asyncio.await(Scene.ShowTutorial("Map01Dialogue"))
    asyncio.await(Scene.ShowMessage("向导", "可以继续了。"))
end)
```

`await` 返回异步操作的最终结果；若操作被取消，任务会立即终止且不再执行后续语句。调用 `asyncio.cancel_task(task)` 仅停止当前任务，共享操作及其他等待者不受影响。任务完成或取消时均会执行 Lua `__close` 清理。

操作 API 详见 [AsyncOperation](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/事件与模块根.md#asyncoperation>)，自定义入口的初始化流程请参阅 [Lua 运行时与模块](<../Lua 与蓝图脚本/Lua 运行时与模块.md#异步操作更新>)。

需要批量读取文件时，`asyncio.start_file_batch(specs)` 可并发读取多个独立文件并按清单顺序交付结果。`asyncio.file_worker_count()` 根据进程可用逻辑处理器数返回默认并发读取上限；每个批次使用的工作线程数不超过该上限，且不超过文件总数（WebGL 使用单个协作式工作单元）。

- **批次规范**：每项规范需提供唯一的 `category` 与 `root`。扫描目录时指定 `suffix`（可选 `excludeSuffix` 或 `recursive = true`）；读取单个文件时将 `file` 设为 `root` 下的相对路径，并省略目录扫描选项。逻辑路径 `Data/...` 支持读取打包数据（包括 `.json` 对应的加密文件），其他根目录则读取物理文件系统。缺少必需输入会导致批次失败。
- **轮询与取消**：`asyncio.poll_file_batch(job, maxItems?, waitForItem?)` 返回进度、按序交付的条目及结构化错误信息，默认每次取一项且不等待。传入正数 `maxItems` 与 `true` 可阻塞等待下一个有序条目或终态；传入 `0` 则仅查询进度而不提取条目。逐帧更新中应使用非阻塞轮询。调用 `asyncio.cancel_file_batch(job)` 可取消任务。
- **任务状态**：将 `state` 与 `asyncio` 常量对比：`FILE_BATCH_SCANNING`（`"scanning"`）、`FILE_BATCH_RUNNING`（`"running"`）、`FILE_BATCH_COMPLETED`（`"completed"`）、`FILE_BATCH_CANCELLING`（`"cancelling"`）、`FILE_BATCH_CANCELLED`（`"cancelled"`）及 `FILE_BATCH_FAILED`（`"failed"`）。`completed`、`cancelled` 与 `failed` 属于批次任务终态；已交付的数据转换对象仍遵循下文的分步转换契约。
- **工作线程解码（`parseData = true`）**：选定的文件名或后缀必须以 `.json` 或 `.ldc` 结尾。工作线程负责解码 JSON 或二进制 LDC（解码失败时报告 `operation = "parse"`），轮询结果返回 `FileBatchDataConversion`，其中 `contentBytes` 表示解码后、转换为 Lua 值前的载荷字节数。
- **增量转换**：在 Lua 逻辑线程上调用 `asyncio.step_file_batch_data(conversion, maxNodes, maxMilliseconds)`（两个预算参数均须为正数）逐步推进 `FileBatchDataConversion`。该方法返回 `completed, processed, data`（`data` 仅在完成时交付一次）。转换语义与 `Engine.loadData` 一致：数组逻辑长度存于 `n`，根节点 `null` 转为 `nil`，数组 `null` 槽位省略键，对象 `null` 字段保留为 `cjson.null`。
- **转换对象生命周期**：调用 `asyncio.clear_file_batch_data` 可幂等释放转换资源；取消任务、垃圾回收与运行时关闭同样会使其失效。转换对象不可跨 Lua 虚拟机或线程使用。
- **原始字节（省略 `parseData` 或设为 `false`）**：`item.content` 保留原始文件字节（包含 LDC 文件头与编码载荷），按需传入 `Engine.decodeData` 解码（蓝图缓存即保留原始字节以延迟解码）。

## 计时与内存诊断

`perfCounter()` 返回单调递增的高精度时间戳（单位：秒）。`processMemoryMB()` 返回当前进程内存占用（单位：MiB）；如需衡量特定值图所占内存而非整个进程内存，请使用 `asizeof`。
