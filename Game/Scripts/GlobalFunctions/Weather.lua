local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local Weather = {}

function Weather.SetWeather(weatherType, power, maxCount)
    power = power == nil and 40 or power
    maxCount = maxCount == nil and 80 or maxCount
    loadGlobalCore().WeatherController.setWeather(weatherType, power, maxCount)
end

function Weather.ClearWeather()
    loadGlobalCore().WeatherController.clearWeather()
end

return Weather
