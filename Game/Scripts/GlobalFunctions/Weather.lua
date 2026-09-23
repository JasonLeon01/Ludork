local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local LocaleCore
local function loadLocaleCore()
    if LocaleCore == nil then
        LocaleCore = require("Source.Locale.Core")
    end
    return LocaleCore
end

local Weather = {}

local function coerceWeatherType(weatherType)
    for name, value in pairs(loadGlobalCore().WeatherType) do
        if weatherType == value or weatherType == name
            or weatherType == loadLocaleCore().ApplyStringLocaleFormat("WEATHER_TYPE_" .. name) then
            return value
        end
    end
    return loadGlobalCore().WeatherType.NONE
end

function Weather.SetWeather(weatherType, power, maxCount)
    power = power == nil and 40 or power
    maxCount = maxCount == nil and 80 or maxCount
    loadGlobalCore().WeatherController.setWeather(coerceWeatherType(weatherType), power, maxCount)
end

function Weather.ClearWeather()
    loadGlobalCore().WeatherController.clearWeather()
end

return Weather
