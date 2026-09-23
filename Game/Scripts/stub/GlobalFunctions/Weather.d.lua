---@meta GlobalFunctions.Weather

local Weather = {}

---@param weatherType GlobalCore.WeatherType | string
---@param power       integer
---@param maxCount    integer
function Weather.SetWeather(weatherType, power, maxCount) end

function Weather.ClearWeather() end

return Weather
