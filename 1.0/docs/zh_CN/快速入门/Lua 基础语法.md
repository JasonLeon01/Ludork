# Lua 基础语法

Ludork 内置 Lua 5.5，本页介绍与 C 不同的语法和行为。引擎扩展见 [Ludork Lua 进阶](<Ludork Lua 进阶.md>)，图形与几何类型见 [SFML 快速入门](<SFML 快速入门.md>)。

## 文件、注释与局部变量

Lua 文件使用 `.lua` 扩展名，注释用 `--` 或 `--[[ ... ]]`。局部变量用 `local` 声明，未初始化时为 `nil`。

```lua
-- 单行注释
local playerName = "Hero"
local level, alive, target = 1, true, nil
```

Lua 的值包括 `nil`、boolean、number、string、function、table、userdata 与 thread。`type(value)` 返回这些内建类别之一。由 table 或 userdata 表示的应用对象，仍然归入它底层的类别。

## 运算符

| 类别 | 运算符 |
|---|---|
| 算术 | `+`、`-`、`*`、`/`、`//`、`%`、`^` |
| 比较 | `==`、`~=`、`<`、`<=`、`>`、`>=` |
| 逻辑 | `and`、`or`、`not` |
| 字符串 | `..` 拼接 |
| 长度 | `#value` |

Lua 只把 `nil` 与 `false` 当作假。`0`、空字符串、空 table 用作条件时都为真。

## 条件分支

```lua
if health <= 0 then
    state = "defeated"
elseif health < maxHealth then
    state = "hurt"
else
    state = "healthy"
end
```

需要把“缺失”与 `false`、`0` 或空值区分开时，应显式比较 `nil`。

## 表（table）

table 既能当数组，也能当字典或记录。数组索引从 1 开始。

```lua
local items = { "Key", "Potion" }
local player = {
    name = "Hero",
    hp = 100,
}

items[#items + 1] = "Gem"
player.hp = player.hp - 10
```

给 table 的键赋值为 `nil` 即删除该键。数组应保持连续：空洞会使 `#` 的结果不可靠，`ipairs` 则在第一个 `nil` 处停止。

## 循环

```lua
for index, item in ipairs(items) do
    useItem(index, item)
end

for key, value in pairs(player) do
    inspectValue(key, value)
end
```

稠密数组用 `ipairs`，字典用 `pairs`。数值循环写作 `for i = first, last, step do ... end`，步长能到达终点时包含终点；while 循环写作 `while condition do ... end`。

## 函数与方法

函数本身也是值，而且可以返回多个结果。实例方法用冒号语法定义，接收者会作为 `self` 传入。

```lua
local function clampHealth(value, maximum)
    return math.max(0, math.min(value, maximum))
end

local Character = {}

function Character:setHealth(value)
    self.health = clampHealth(value, self.maxHealth)
end

local value, found = findItem("Key")
```

`object:method(value)` 等价于 `object.method(object, value)`。定义和调用实例方法时都统一用冒号形式。

## 模块

模块就是一个把公开内容作为返回值导出的 Lua 文件。`require` 的路径以 `Scripts` 为根，用点号分隔目录层级，并且不带扩展名。

```lua
-- Scripts/Source/Score.lua
local Score = {}

function Score.Add(current, amount)
    return current + amount
end

return Score
```

```lua
local Score = require("Source.Score")

local total = Score.Add(10, 5)
```

项目加载顺序和 `require` 约定见 [Lua 运行时与模块](<../Lua 与蓝图脚本/Lua 运行时与模块.md>)。

## 错误

必须成立的条件用 `assert(condition, message)` 检查，需要主动失败时用 `error(message)`。项目必需的数据必须在源头直接失败，而不是悄悄产生不完整的状态。

## 相关页面

- [默认模板概述](<../Lua 与蓝图脚本/Game 玩法/默认模板概述.md>) — Game 项目如何使用这些规则。
