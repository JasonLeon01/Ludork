local GlobalCore = require("GlobalCore")
local LocaleCore = require("Source.Locale.Core")

local WeatherController = GlobalCore.WeatherController
local WeatherType = GlobalCore.WeatherType
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local Weather = {}

local function coerceWeatherType(weatherType)
    for name, value in pairs(WeatherType) do
        if weatherType == value or weatherType == name or weatherType == LOC("WEATHER_TYPE_" .. name) then
            return value
        end
    end
    return WeatherType.NONE
end

function Weather.SetWeather(weatherType, power, maxCount)
    power = power == nil and 40 or power
    maxCount = maxCount == nil and 80 or maxCount
    WeatherController.setWeather(coerceWeatherType(weatherType), power, maxCount)
end

function Weather.ClearWeather()
    WeatherController.clearWeather()
end

return Weather
