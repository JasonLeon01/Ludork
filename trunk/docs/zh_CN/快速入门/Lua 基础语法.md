# Lua 基础语法

Ludork 运行 Lua 5.5。本指南介绍与 C 语言不同的语法与运行时行为；Ludork 的 Standard 扩展请参阅 [Ludork Lua 进阶](<Ludork Lua 进阶.md>)，图形与几何类型请参阅 [SFML 快速入门](<SFML 快速入门.md>)。

## 文件、注释与局部变量

Lua 脚本使用 `.lua` 扩展名，注释使用 `--`（单行）或 `--[[ ... ]]`（多行）。局部变量通过 `local` 声明，未初始化的变量求值为 `nil`。

```lua
-- 单行注释
local playerName = "Hero"
local level, alive, target = 1, true, nil
```

Lua 值分为八种内置类别：`nil`、boolean、number、string、function、table、userdata 与 thread。`type(value)` 返回对应类别的名称；以 table 或 userdata 表示的应用对象仍报告其底层 Lua 类别。

## 运算符

| 类别 | 运算符 |
|---|---|
| 算术 | `+`、`-`、`*`、`/`、`//`、`%`、`^` |
| 比较 | `==`、`~=`、`<`、`<=`、`>`、`>=` |
| 逻辑 | `and`、`or`、`not` |
| 字符串 | `..` 拼接 |
| 长度 | `#value` |

Lua 仅将 `nil` 与 `false` 视为假值；`0`、空字符串（`""`）与空 table（`{}`）在条件判断中均求值为 `true`。

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

需要将“值缺失”与 `false`、`0` 或空值区分开时，请显式与 `nil` 比较。

## 表（table）

table 可同时充当数组、字典或记录。数组索引从 1 开始。

```lua
local items = { "Key", "Potion" }
local player = {
    name = "Hero",
    hp = 100,
}

items[#items + 1] = "Gem"
player.hp = player.hp - 10
```

将 table 的键赋值为 `nil` 即可删除该键。数组应保持连续：空洞会导致 `#` 的结果不可靠，且 `ipairs` 会在首个 `nil` 处终止遍历。

## 循环

```lua
for index, item in ipairs(items) do
    useItem(index, item)
end

for key, value in pairs(player) do
    inspectValue(key, value)
end
```

连续数组使用 `ipairs` 遍历，字典使用 `pairs` 遍历。数值循环写作 `for i = first, last, step do ... end`，在步长可达时包含起止两个端点；`while` 循环写作 `while condition do ... end`。

## 函数与方法

函数是一等公民，且支持返回多个结果。实例方法使用冒号语法定义，自动将接收者作为 `self` 参数传入。

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

`object:method(value)` 等价于 `object.method(object, value)`。定义与调用实例方法时请统一使用冒号语法。

## 模块

模块是返回公开值的 Lua 文件。调用 `require` 时，路径以 `Scripts` 目录为根，使用点号分隔层级，并省略 `.lua` 扩展名。

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

项目加载顺序与 `require` 约定请参阅 [Lua 运行时与模块](<../Lua 与蓝图脚本/Lua 运行时与模块.md>)。

## 错误

必须满足的前置条件使用 `assert(condition, message)` 检查，显式触发失败使用 `error(message)`。项目必需的数据应在源头直接失败，而非静默产生不完整状态。
