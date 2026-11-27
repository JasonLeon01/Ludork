# 元数据结构与 Decorator

`_METADATA` 是描述蓝图字段、函数签名、编辑器控件与执行协议的纯数据 Schema，用于向编辑器说明 Lua 与 C++ 类型的可编辑成员及蓝图节点接口。每个元数据文件都必须返回一个名为 `_METADATA` 的纯数据表。

```lua
local _METADATA = {
    SoundFilter = {
        moduleReturn = true,
        attrs = {
            "volume",
        },
        volume = {
            type = "float",
            default = 1.0,
        },
        apply = {
            type = "function",
            parameters = {
                "self",
                "source",
                self = { "Source.SoundFilter", "SoundFilter" },
                source = { "Engine", "SoundSource" },
            },
            default = { [1] = "self" },
            ["return"] = {},
            Pure = false,
        },
    },
}

return _METADATA
```

## 类型与字段

每张类型表通过有序的 `attrs` 数组列出可编辑字段；表自身的键顺序没有意义。每个字段必须声明 `type`，且仅当蓝图已保存该字段的值，或元数据提供了 `default` 时，才会以普通值的形式显示。

- **基础类型**：`bool`、`int`、`float`、`string`、`function`、`event`、`any` 与 `Pair`。
- **SFML 与 Core 类型**：SFML 值使用 `sf.` 前缀。Core 与 Lua 类型写作 `{ "Module", "Type" }`，表示数组时在第二项末尾加上 `[]`。
- **递归容器**：`T[]` 表示可变长度列表，`Dict[string, T]` 表示字符串键字典，`Tuple[T1, T2, ...]` 表示固定长度的异构数组（例如 `Dict[string, Tuple[string, any]]`、`Dict[string, int[]]` 或 `int[][]`）。
- **组件与复合控件**：`component = true` 用于标记组件字段。复合控件按实际类型自动选择（向量、颜色、矩形与 `Pair`，其中 `Pair` 是二分量数值控件，并非异构的 `Tuple`）。`RectRangeVars` 为 `sf.IntRect` 附加纹理区域选择语义。

大写的 `Dict[...]` 与 `Tuple[...]` 仅表示元数据 Schema，不是 Standard 运行时中小写 `dict` 与 `tuple` 容器的构造函数。元数据默认值必须是可转换为 JSON 的纯数据，应使用普通 Lua 表，不得使用原生 `list`、`tuple` 或 `dict` 值。接受原生容器的运行时引脚必须声明限定类型（例如 `{ "_G", "list" }`）或将其纳入联合类型；`T[]` 引脚接受 Lua 数组，传入原生 `list` 需使用显式转换节点。

无论容器是否为空，序列化形态始终由声明的 Schema 决定：`Dict` 序列化为 JSON 对象，列表与 `Tuple` 序列化为 JSON 数组。凡已知形状的元数据均应使用显式递归类型。运行时调用必须传入与声明匹配的类型，转换规则见[运行时值边界](<../../C++ 原生开发/运行时值边界.md>)。

### 值字面量与未设置输入

- `string` 字段原样保留全部文本，包括 `"nil"`；已保存容器内部嵌套的字符串同样按字面处理。
- 整值表达式只求值一次，因此 `any` 表达式中的 `nil` 会求值为 Lua `nil`。
- 未设置的节点输入保存为 JSON `null`，若声明了默认值则回退至该默认值。JSON 值编辑器可直接输入 `null`；清空函数表达式可使其保持未设置。
- `any` 输入以文本方式编辑：失去焦点时，可解析为 JSON 字面量（如数字）的文本按对应字面量保存，其余文本保存为表达式字符串。

### 普通文本节点输入

**转换为普通输入框**会绕过编辑器选择器与数值控件，但仍强制执行同一 Schema：

| Schema 类型 | 接受的普通文本输入 |
|---|---|
| `string`、`file` | 原样保留全部文本，包括空白、空输入与 `null`。 |
| `int` | 无小数部分的有符号 64 位整数。留空表示未设置。 |
| `float`、`double`、`number` | 有限数字，小数点固定使用 `.`。留空表示未设置。 |
| `bool` | `true` 或 `false`。留空表示未设置。 |
| 向量、`Pair` | 合法的 JSON 分量数组（例如 `[1, 2]`）。留空表示未设置。 |
| `sf.Color` | 由四个 `0..255` 整数组成的 JSON 数组。留空表示未设置。 |
| `sf.FloatRect` | 由四个数字组成的 JSON 数组。留空表示未设置。 |
| `sf.IntRect` | 嵌套 JSON 数组，例如 `[[0, 0, 32, 32]]`。留空表示未设置。 |
| 容器与联合类型 | 合法的 JSON；联合类型须保留声明的 `$type`/`$value` 包装（容器内部亦然）。留空表示未设置。 |
| `any` | 沿用标准的 JSON 字面量或表达式转换，保留空白文本与带引号的 JSON 字符串；输入 `null` 表示未设置。 |
| 函数、事件、对象 | 沿用标准的运行时表达式与对象引用语义。 |

无效的普通文本仅作为编辑会话草稿保留，绝不会写入参数值，且在修正前会阻止保存及切换回类型输入框。普通文本模式与草稿均仅限当前窗口，不修改元数据或蓝图 JSON。

## 蓝图属性声明

蓝图 JSON 在 `attrs` 中保存属性值，并在同级 `attrDefs` 对象中声明本蓝图新增的字段。未新增字段时可省略 `attrDefs`。每条声明均需提供 `type`，支持与元数据相同的基础类型、限定类型、枚举与递归 Schema；`file` 类型还可声明 `base` 限定资产选择目录，但字段值仍保存完整的 `/Game/Assets/...` 路径。

```json
{
  "attrDefs": {
    "rewardCount": { "type": "int" },
    "stairChoice": { "type": { "enum": "Enums.StairDirection" } },
    "offsets": { "type": { "list": "sf.Vector2i" } },
    "icon": { "type": "file", "base": "Images" }
  },
  "attrs": {
    "rewardCount": 3,
    "stairChoice": "Up",
    "offsets": [[0, 1]],
    "icon": ""
  }
}
```

`attrDefs` 条目仅包含 `type` 及可选文件 `base`，值与初始默认值一律写在 `attrs` 中。声明可省略值，编辑器与运行时会始终保留声明类型，不会根据值进行推断。

- **继承规则**：本地 `attrDefs` 之前，系统会先解析 Lua/原生元数据、当前启用的 Script Mixin 以及祖先蓝图的声明。子类同时继承类型与默认值：可在 `attrs` 中覆盖值，但不得在 `attrDefs` 中重复声明或更改继承类型。本地字段名不得与已有字段、方法或运行时保留名冲突（例如继承的 `speed` 或 Mixin 的 `needKeyCount` 属于值覆盖，无需重新声明）。
- **严格校验**：所有已保存属性及带类型记录的嵌套成员均须有声明且符合对应 Schema。未知字段、缺失声明、无效 Schema 或声明冲突均会导致加载失败，并指出蓝图路径与字段名；编辑器与运行时不会自动推断缺失声明，也不会转换旧的无类型数据。
- **其他图资产**：地图 `BPClassVarChanged` 条目只保存值，并按 Actor 类的继承 Schema 校验。纯图实现的公共函数与通用数据成员图不具备类声明。

## 枚举 Schema

枚举 Schema 引用 `Scripts/Enums` 下的某个模块，其源码包含一个标量字面量表及对应 EmmyLua 注解（参见[枚举模块](<../Lua 运行时与模块.md#枚举模块>)）。可在任意字段、参数、返回值或嵌套容器/联合类型 Schema 中按 `require` 路径引用：

```lua
stairDirection = {
    type = { enum = "Enums.StairDirection" },
    default = "None",
}
```

### 源码模块与标量类型规则

- **允许的语法**：枚举模块可直接返回字面量表，或先声明一个局部字面量表再返回该变量。允许注释、注解、可选分号以及包裹单个标量字面量的一层括号；禁止函数调用（含 `require`）、表达式计算、表修改、返回其他变量及额外语句。
- **键与值类别**：键必须为非空字符串，全部值必须属于同一类别：字符串、布尔值或有限数字。未指定 `valueType` 时，全整数字面量表解析为 `int`，只要包含任意浮点字面量（含 `1.0`）即解析为 `float`。
- **显式 `valueType`**：可选字段 `valueType`（`"string"`、`"bool"`、`"int"` 或 `"float"`，如 `{ enum = "Enums.GeneralData.Item", valueType = "string" }`）允许空枚举表并预先固定标量类型。非空常量必须与声明一致（显式 `float` 接受整数字面量）。未声明 `valueType` 的空模块、常量类型冲突、未知 Schema 属性或模块缺失均会校验失败（嵌套容器中亦然）。`valueType` 仅约束标量类型，不检查取值是否属于当前选项列表。

### 选择器、默认值与实时解析

编辑器选择器按声明顺序展示键名，并直接保存底层标量值（而非键名或 Lua 表达式），无需使用 `Meta.DropBox`。多个键取值相同时，回显第一个匹配的键。当前常量表中已移除的旧值会继续保留并提示“未知值”，只要底层标量类型匹配即合法；枚举不在运行时强制检查成员资格。

- **默认值**：显式声明的 `default` 优先作为标量默认值。未声明时，新节点输入与容器项取第一个选项；若枚举为空，则取标量类型默认值（`""`、`false` 或 `0`）。未声明默认值的属性在写入值之前仅提供 Schema。字符串枚举选择器包含 **— 不选择 —** （保存为 `""`），未知的非空取值仍会显示并保留。
- **普通枚举**：每当字段显示或下拉选择器打开（包括窗口重新获得焦点）时，编辑器会直接重读源文件，无需执行玩法模块。
- **项目受管枚举**：编辑器在源码生成前直接从当前项目目录（含未保存修改）实时解析以下四类 `valueType = "string"` 枚举：

| 受管枚举 | 选项来源 |
|---|---|
| `Enums.GeneralDataKey` | 通用数据类型键 |
| `Enums.GeneralData.<TypeName>` | 指定通用数据类型的成员 |
| `Enums.Animation` | 动画键 |
| `Enums.Particle` | 粒子键 |

已删除的受管模块即使磁盘上仍有旧生成文件，也会触发校验错误。枚举无效时，错误信息会指明模块路径，并保留字段当前值。

### 枚举键字典与普通文本

字典 Schema 可独立于值类型声明字符串枚举键：

```lua
type = {
    dict = "int",
    key = { enum = "Enums.GeneralData.Item", valueType = "string" },
}
```

- 省略 `key` 时使用普通字符串键。显式声明的 `key` 只能是 `"string"` 或声明了 `valueType = "string"` 的枚举；数值、布尔、容器及未声明底层类型的枚举键均会被拒绝。
- 字典值保留各自的递归 Schema，JSON 序列化为以所选标量字符串为键的对象。
- 新增枚举键行时，需选定非空且唯一的键后才会更新保存数据；重复键会被拒绝。`DictKeyMeta` 可细化其他键控件，但不会替换声明的键类型。
- **普通文本模式与连线**：使用**转换为普通输入框**直接编辑底层标量值（例如整数枚举需输入数字而非显示键名）。Schema 会保留枚举身份（包括联合类型的 `$type` 描述），实际存储值为标量。[连线的类型规则](<执行流、事件与变量.md#连线的类型规则>)同样按底层标量类型判断兼容性。

原生枚举与带标记的常量映射会生成同等结构的模块，如 `Enums.Engine.Direction`、`Enums.GlobalCore.WeatherType` 与 `Enums.sf.Keyboard.Scan`，参见[生成枚举模块](<../../C++ 原生开发/生成元数据与 Stub.md#生成枚举模块>)。

## 联合类型 Schema 与保存字面量

联合类型以 `{ union = { T1, T2 } }` 列出允许的分支（显示为 `Union[T1, T2]`），并可与 `{ list = T }`、`{ dict = T }`、`{ tuple = { T1, T2 } }` 组合——这三种写法分别是 `T[]`、`Dict[string, T]` 与 `Tuple[...]` 的结构化形式。

显式声明的默认值决定初始分支与取值；未声明时，新节点会选择第一个具有有效字面量默认值的分支，无默认值的分支保持未设置。切换分支会将值重置为新分支的默认值。

```lua
type = { union = { "sf.Vector2f", "sf.Vector2u", "nil" } },
default = { ["$type"] = "sf.Vector2u", ["$value"] = { 64, 32 } },
```

上述默认值在 JSON 中保存为 `{ "$type": "sf.Vector2u", "$value": [64, 32] }`。`$type` 以限定类型名或递归容器 Schema（例如 `{ "list": "sf.Vector2u" }`）标识具体分支，从而区分 JSON 形态相同的不同类型。

显式 `nil` 在 JSON 中序列化为 `{ "$type": "nil", "$value": null }`，在 Lua 元数据中写作 `{ ["$type"] = "nil" }`，与未设置值（回退至默认值）语义不同。仅在 Schema 声明联合类型的位置（包括嵌套联合类型）使用 `$type`/`$value` 包装。运行时调用接收到的是解包后的实际值；非法值会报告受影响的字段或参数。

## 继承

`bases` 是直接元数据基类的有序数组，每项写作 `{ "Module", "Type" }`。仅列出参与编辑器字段继承的基类。Script Mixin 元数据不得声明 `bases`。

当 Lua 模块直接返回其类本身时，在该类的元数据表中设置 `moduleReturn = true`，并确保该元数据文件仅包含这一个类型。类选择器与新建蓝图的父类随后将使用模块路径（例如 `Source.MapActors.Enemy`）。作为具名成员导出的类不声明 `moduleReturn`，沿用完整引用（例如 `Engine.Actor`）。元数据中的类型引用——包括 `bases`、字段与节点引脚——始终使用显式的 `{ "Module", "Type" }` 二元组（例如 `{ "Source.MapActors.Enemy", "Enemy" }`）。

## 函数与事件

`parameters` 与 `["return"]` 同时包含有序名称数组与名称到类型的映射条目。可选的 `default` 数组按索引与 `parameters` 对齐。`Pure` 须显式声明；`ExecSplit`、`Latent`、`LoopNode` 与 `type = "event"` 用于声明节点图的运行时执行协议。

- **实例方法**：所有普通实例方法均将 `"self"` 声明为第一个参数，类型写为所属类的显式 `{ "Module", "Type" }` 引用，并设置 `default[1] = "self"`（例如 `{ "Source.Scenes.SceneMap", "Scene" }`）。该接收者引脚在任何蓝图上下文中均可见，参见[实例方法的接收者](<执行流、事件与变量.md#实例方法的接收者>)。计算默认值索引与保存参数位置时须计入接收者槽位。静态函数与事件省略 `"self"`。Lua 冒号调用签名与原生方法声明保持常规参数列表。
- **引脚顺序与返回值**：数组部分决定引脚顺序，字符串键决定引脚类型。`default` 中的 `nil` 槽位表示无默认值。在 `["return"]` 中具名声明的 `nil` 类型仍会生成输出引脚，只有空表 `["return"] = {}` 才表示无输出。仅 `Pure = true` 会移除执行引脚。
- **未设置默认值**：若参数初始值必须保持 `nil` 而非由编辑器生成字面量，请使用 `defaultUnset = { "parameterName" }`（例如 `Scene.recordActorPosition` 声明 `defaultUnset = { "position" }`，以保留省略坐标时默认记录 Actor 当前位置的语义）。`defaultUnset` 按参数名指定，不受接收者索引偏移影响。

静态函数只需声明自身参数：

```lua
calculate = {
    type = "function",
    parameters = {
        "value",
        "clamp",
        value = "float",
        clamp = "bool",
    },
    default = {
        nil,
        true,
    },
    ["return"] = {
        "result",
        result = "float",
    },
    Pure = true,
}
```

## 执行声明

- **`ExecSplit`**：固定执行输出的顺序，并将其映射到运行时返回值，例如 `ExecSplit = { "success", "fail", success = true, fail = false }`。
- **`Latent = true`**：标记节点图挂起。有序的 `LatentStates` 将恢复输出映射到阶段值，且返回值类型必须为 `{ "Engine", "AsyncOperation" }`，参见[执行语义](<执行流、事件与变量.md#执行语义>)。
- **`Loop = true`**：标记循环节点；`LoopNode = "ForEach"` 或 `"ForLoop"` 指定运行时迭代协议。
- **`type = "event"`**：以相同的参数/返回值格式创建事件入口节点。未显式声明 `ExecSplit` 时，事件默认获得 `ExecSplit = { "default", default = "nil" }`（字符串 `"nil"` 是默认分支的元数据标记）。

> **重要**：执行声明是对节点图运行时的正式契约。Lua 或 C++ 实现必须实际返回声明的分支值、恢复声明的 Latent 状态，或驱动声明的循环协议。

## 编辑器名称与 Decorator

类型、字段、函数、事件、引脚与蓝图变量的编辑器显示标签均由其标识符自动推导（例如 `textureRect` 显示为**Texture Rect**）。元数据键名、运行时成员名与已保存的蓝图名称保持不变。

普通成员禁止声明 `DisplayName`、`DisplayDesc`、`VariableDisplayNames`、`VariableDisplayDescs`、`ParameterDisplayNames` 或 `ParameterDisplayDescs`；编辑器绝不会求值 `LOC(...)` 或其他表达式来推导 Schema 标签或说明。唯一例外是 `ADD`、`IADD` 等运算符节点，可声明直接字面量 `Meta.DisplayName`（如 `+`、`+=`），但不得包裹 `LOC(...)`，也不得声明默认 `DisplayDesc`。

`Meta` 提供无法从类型推导的编辑器语义，包括 `DropBox`、`PathVars`、`PathRoot`、`PathFilter`、`ProgressVars`、`SliderVars`、`RangeVars`、`MoveRouteVars`、`Transfer`、`BlueprintClassVars`、`CommonFunctionVars`、`ConfigVars`、`InstVar`、`InstVarValue`、`DictKeyMeta`、`ItemMeta`、`InvalidVars` 与 `RectRangeVars`。请直接保留 Decorator 名称及有序数据，切勿包裹在自造的 `fields`、`nodes` 或 `events` 容器中。

### 资源与数据路径选择器

| Decorator | 行为 |
|---|---|
| `PathVars` | 选择器根目录。对于游戏资源，提供完整逻辑根目录（如 `/Game/Assets/Sounds`），字段保存完整逻辑路径。 |
| `PathRoot = "Project"` | 选择 Script Mixin 等原生项目文件。 |
| `PathRoot = "Data"` | 在项目相对 `PathVars` 目录（如 `Data/Subtitles`）下选择数据文件，并保存完整的 `Data/...` 路径。引用必须位于选择器目录内，不得包含路径穿越段。 |
| `PathFilter` | 文件通配过滤模式，例如 `PathFilter = "*.json"`。 |

在函数元数据中可通过表映射针对具名参数指定 Decorator，例如 `PathRoot = { subtitleFileName = "Data" }`。

### 游戏变量选择器（`InstVar` 与 `InstVarValue`）

- **`InstVar`**：将字符串字段或参数标记为游戏变量名，用可搜索选择器替代文本输入，选项来自当前游戏变量声明（含未保存修改）。设为 `InstVar = true` 允许全部类型；设为 `InstVar = { types = { "int", "float" } }` 限定元数据内置类型；在函数元数据中使用 `InstVar = { "valueName" }` 指定目标参数。
- **`InstVarValue`**：将另一字段或参数关联至所选游戏变量的类型，使其采用对应的编辑器控件。在同级字段中传入字段名字符串（如 `InstVarValue = "openConditionName"`），在函数元数据中传入参数映射表（如 `InstVarValue = { value = "valueName" }`）。该机制仅影响编辑器控件，运行时读写仍操作实际游戏变量表。

```lua
openConditionName = {
    type = "string",
    Meta = {
        InstVar = {
            types = {
                "int",
                "float",
            },
        },
    },
}
openConditionVal = {
    type = "int",
    Meta = {
        InstVarValue = "openConditionName",
    },
}
```

### 容器与控件 Decorator

容器 Decorator 仅在紧邻的容器层细化编辑控件，不改变 Schema 或 JSON 存储形态：

- **`DictKeyMeta`**：将 Decorator 应用于每个字符串键。
- **`ItemMeta`**：将 Decorator 应用于每个直接列表项或字典值。若元素为 `Tuple`，可通过 `ItemMeta.TupleMeta` 按从 1 开始的槽位索引分别指定 Decorator。
- **`DropBox`**：在不存在共享枚举模块时提供有序字面量下拉选项；若选项属于共享枚举，优先使用枚举 Schema。
- **`"$dictKey"`**：在字典值的 `TupleMeta` 内，`InstVarValue = "$dictKey"` 解析为当前行的字典键。该保留引用在行外无效。
- **`InvalidVars`**：列出对蓝图属性编辑器与序列化器隐藏的字段。
- **`RectRangeVars`**：将 `sf.IntRect` 关联到图像区域选择器所需的纹理/范围字段。

```lua
afterBattleVarChanges = {
    type = "Dict[string, Tuple[string, any]]",
    default = {},
    Meta = {
        DictKeyMeta = {
            InstVar = {
                types = { "int", "float" },
            },
        },
        ItemMeta = {
            TupleMeta = {
                [1] = {
                    DropBox = { "=", "+", "-", "*", "/", "//", "%", "**" },
                },
                [2] = {
                    InstVarValue = "$dictKey",
                },
            },
        },
    },
}
```

该字段保存为值为二元素数组的 JSON 对象。键选择器仅列出已声明的 `int` 与 `float` 游戏变量，元组第 1 项提供运算符下拉菜单，第 2 项使用所选变量对应的数值编辑器。这些 Decorator 仅约束创作阶段，运行时游戏变量表不会强制检查声明类型。

## 安全规则

元数据必须是纯数据：严禁包含函数、userdata、协程（thread）、循环引用、非有限数值及任何副作用，且默认值必须可转换为 JSON。元数据字符串绝不会被求值以生成标签；运行时本地化及本地化选项值属于独立职责。

Core 元数据由 `BIND_*` 注解自动生成，切勿手工编辑。参见[生成元数据与 Stub](<../../C++ 原生开发/生成元数据与 Stub.md>)。
