# GlobalFunctions.Weather

`Weather` 声明设置与清除天气效果的节点。

Metadata 来源：`Scripts/GlobalFunctions/Weather_meta.lua`

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `SetWeather` | `function` | weatherType: `{ enum = "Enums.GlobalCore.WeatherType" }`; power: int = 40; maxCount: int = 80 | — | 默认执行输出 |
| `ClearWeather` | `function` | — | — | 默认执行输出 |

选择器显示生成模块 `Enums.GlobalCore.WeatherType` 中的 `NONE`、`RAIN`、`STORM` 和 `SNOW`，保存对应数字 `0`、`1`、`2` 和 `3`。Lua 调用者直接传入枚举值：

```lua
local WeatherType = require("Enums.GlobalCore.WeatherType")
local Weather = require("GlobalFunctions.Weather")

Weather.SetWeather(WeatherType.STORM, 30, 50)
```

节点把该值直接交给 `GlobalCore.WeatherController.setWeather`，名称与本地化标签不作为运行时参数。上例在蓝图 JSON 中保存为 `[2, 30, 50]`。
