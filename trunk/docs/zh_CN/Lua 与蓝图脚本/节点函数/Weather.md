# GlobalFunctions.Weather

`GlobalFunctions.Weather` 提供用于启用和清除场景天气效果的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Weather_meta.lua`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `SetWeather` | `weatherType: Enums.GlobalCore.WeatherType`, `power: int = 40`, `maxCount: int = 80` | — | 启用指定的天气效果。`Enums.GlobalCore.WeatherType` 枚举选择器。 |
| `ClearWeather` | — | — | 清除当前生效的天气效果。 |

枚举选择器显示生成模块 `Enums.GlobalCore.WeatherType` 中的 `NONE`、`RAIN`、`STORM` 与 `SNOW`，并分别保存对应的数值 `0`、`1`、`2` 和 `3`。Lua 调用方直接传入枚举值即可：

```lua
local WeatherType = require("Enums.GlobalCore.WeatherType")
local Weather = require("GlobalFunctions.Weather")

Weather.SetWeather(WeatherType.STORM, 30, 50)
```

该节点会将数值转发给 `GlobalCore.WeatherController.setWeather`；名称与本地化标签不作为运行时参数。上例在蓝图 JSON 中保存为 `[2, 30, 50]`。
