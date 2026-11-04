# GlobalFunctions.Weather

`Weather` declares the nodes that set and clear the weather effect.

Metadata source: `Scripts/GlobalFunctions/Weather_meta.lua`

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `SetWeather` | `function` | weatherType: `{ enum = "Enums.GlobalCore.WeatherType" }`; power: int = 40; maxCount: int = 80 | — | Default execution output |
| `ClearWeather` | `function` | — | — | Default execution output |

The selector displays `NONE`, `RAIN`, `STORM` and `SNOW` from the generated `Enums.GlobalCore.WeatherType` module and stores their numeric values `0`, `1`, `2` and `3`. Lua callers pass the enum value directly:

```lua
local WeatherType = require("Enums.GlobalCore.WeatherType")
local Weather = require("GlobalFunctions.Weather")

Weather.SetWeather(WeatherType.STORM, 30, 50)
```

The node forwards the value to `GlobalCore.WeatherController.setWeather`; names and localised labels are not runtime arguments. Blueprint JSON stores the example above as `[2, 30, 50]`.
