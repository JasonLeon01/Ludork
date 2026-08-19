local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local Locale = require("Source.Locale.Core")

local MainConfig = {}
local DEFAULT_LANGUAGE = "en_GB"
local DEFAULT_DISPLAY_SCALE = 1.0
local DISPLAY_SCALE_EPSILON = 0.0001
local DISPLAY_SCALE_PRESETS = { 0.0, 1.0, 1.25, 1.5, 1.75, 2.0 }
local DEFAULT_MAIN_ITEMS = {
    { "script", "Scripts/Entry.lua" }, { "language", DEFAULT_LANGUAGE }, { "scale", "1.0" }, { "framerate", "120" },
    { "verticalsync", "true" }, { "musicon", "true" }, { "soundon", "true" }, { "voiceon", "true" },
    { "musicvolume", "100.00" }, { "soundvolume", "100.00" }, { "voicevolume", "100.00" }
}

MainConfig.SupportedLanguages = { "en_GB", "zh_CN" }

local function isSupportedLanguage(language)
    for _, supportedLanguage in ipairs(MainConfig.SupportedLanguages) do
        if language == supportedLanguage then
            return true
        end
    end
    return false
end

local function getInitialLanguage()
    local language = Locale.ResolveLanguage(locale.getdefaultlocale())
    if isSupportedLanguage(language) then
        return language
    end
    return DEFAULT_LANGUAGE
end

local function getIniFilePath()
    if os.getenv("LUDORK_EDITOR") == "1" then
        return "./Main.ini"
    end
    return Engine.getMainIniPath()
end

local function createMainIni(iniFilePath, iniFile)
    iniFile:add_section("Main")
    for _, item in ipairs(DEFAULT_MAIN_ITEMS) do
        iniFile:set("Main", item[1], item[2])
    end
    iniFile:set("Main", "language", getInitialLanguage())
    iniFile:write(iniFilePath)
end

local function scaleFits(scale, maximumScale)
    return maximumScale == nil or scale <= maximumScale + DISPLAY_SCALE_EPSILON
end

local function normalizeConfiguredScale(configuredScale)
    if configuredScale == nil or configuredScale ~= configuredScale or configuredScale < 0.0
        or configuredScale == math.huge or configuredScale == -math.huge then
        return DEFAULT_DISPLAY_SCALE
    end
    return configuredScale
end

function MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and (maximumScale ~= maximumScale or maximumScale == math.huge
        or maximumScale == -math.huge) then
        maximumScale = nil
    end
    configuredScale = normalizeConfiguredScale(configuredScale)
    local values = { 0.0 }
    local highestPreset = 0.0
    local matchingPreset = nil
    for index = 2, #DISPLAY_SCALE_PRESETS do
        local preset = DISPLAY_SCALE_PRESETS[index]
        if scaleFits(preset, maximumScale) then
            values[#values + 1] = preset
            highestPreset = preset
            if configuredScale == preset then
                matchingPreset = preset
            end
        end
    end
    if configuredScale == 0.0 then
        return values, 0.0
    end
    if not scaleFits(configuredScale, maximumScale) then
        return values, highestPreset
    end
    if matchingPreset ~= nil then
        return values, configuredScale
    end
    local insertIndex = #values + 1
    for index = 2, #values do
        if configuredScale < values[index] then
            insertIndex = index
            break
        end
    end
    table.insert(values, insertIndex, configuredScale)
    return values, configuredScale
end

function MainConfig.loadOrCreate()
    local iniFilePath = getIniFilePath()
    local iniFile = configparser.ConfigParser()
    if CoreSystem.exists(iniFilePath) then
        iniFile:read(iniFilePath)
    else
        createMainIni(iniFilePath, iniFile)
    end
    return iniFilePath, iniFile
end

return MainConfig
