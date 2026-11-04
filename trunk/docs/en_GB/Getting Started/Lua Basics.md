# Lua Basics

Ludork runs Lua 5.5. This page covers the syntax and behaviour that differ from C. See [Ludork Lua Advanced](<Ludork Lua Advanced.md>) for Ludork's additions and [SFML Quickstart](<SFML Quickstart.md>) for graphics and geometry types.

## Files, comments and local variables

Lua files use `.lua`; comments use `--` or `--[[ ... ]]`. Declare local variables with `local`; uninitialised variables are `nil`.

```lua
-- One line
local playerName = "Hero"
local level, alive, target = 1, true, nil
```

Lua values include `nil`, booleans, numbers, strings, functions, tables, userdata and threads. `type(value)` returns one of these built-in categories. Application objects that are represented by tables or userdata still report that underlying category.

## Operators

| Kind | Operators |
|---|---|
| Arithmetic | `+`, `-`, `*`, `/`, `//`, `%`, `^` |
| Comparison | `==`, `~=`, `<`, `<=`, `>`, `>=` |
| Logic | `and`, `or`, `not` |
| Strings | `..` for concatenation |
| Length | `#value` |

Lua treats only `nil` and `false` as false. Values such as `0`, an empty string and an empty table are true in a condition.

## Conditions

```lua
if health <= 0 then
    state = "defeated"
elseif health < maxHealth then
    state = "hurt"
else
    state = "healthy"
end
```

Use explicit `nil` comparisons when absence must differ from `false`, `0` or an empty value.

## Tables

A table can act as an array, dictionary or record. Array indices start at 1.

```lua
local items = { "Key", "Potion" }
local player = {
    name = "Hero",
    hp = 100,
}

items[#items + 1] = "Gem"
player.hp = player.hp - 10
```

Assigning `nil` removes a table key. Keep arrays dense: holes make `#` unreliable, and `ipairs` stops at the first `nil`.

## Loops

```lua
for index, item in ipairs(items) do
    useItem(index, item)
end

for key, value in pairs(player) do
    inspectValue(key, value)
end
```

Use `ipairs` for dense arrays and `pairs` for dictionaries. Numeric loops use `for i = first, last, step do ... end` and include both endpoints when reached by the step; while loops use `while condition do ... end`.

## Functions and methods

Functions are values, and they may return multiple results. Define instance methods with colon syntax, which passes the receiver as `self`.

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

`object:method(value)` is equivalent to `object.method(object, value)`. Define and call instance methods with the colon form consistently.

## Modules

A module is a Lua file that returns its public value. Require it by its path below `Scripts`, using dots and no extension.

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

Project loading order and `require` conventions are in [Lua Runtime and Modules](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md>).

## Errors

Use `assert(condition, message)` for required conditions and `error(message)` for an explicit failure. Required project data must fail at its source rather than silently produce partial state.

## Related pages

- [Default Template Overview](<../Lua and Blueprint Scripting/Default Gameplay/Default Template Overview.md>) — how the Game project uses these rules.
