# GlobalFunctions.Weather

`GlobalFunctions.Weather` provides Blueprint nodes for activating and clearing scene weather effects.

**Metadata source:** `Scripts/GlobalFunctions/Weather_meta.lua`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `SetWeather` | `weatherType: Enums.GlobalCore.WeatherType`, `power: int = 40`, `maxCount: int = 80` | — | Activates a weather effect. `Enums.GlobalCore.WeatherType` selector. |
| `ClearWeather` | — | — | Clears the active weather effect. |

The selector displays `NONE`, `RAIN`, `STORM` and `SNOW` from the generated `Enums.GlobalCore.WeatherType` module and stores their numeric values `0`, `1`, `2` and `3`. Lua callers pass the enum value directly:

```lua
local WeatherType = require("Enums.GlobalCore.WeatherType")
local Weather = require("GlobalFunctions.Weather")

Weather.SetWeather(WeatherType.STORM, 30, 50)
```

The node forwards the value to `GlobalCore.WeatherController.setWeather`; names and localised labels are not runtime arguments. Blueprint JSON stores the example above as `[2, 30, 50]`.
