local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")
local Locale = require("Source.Locale.Core")

local System = GlobalCore.System

local MainConfig = {}
local DEFAULT_LANGUAGE = "en_GB"
local DEFAULT_DISPLAY_SCALE = 1.0
local DEFAULT_MAXIMUM_RENDER_SCALE = 2.0
local DISPLAY_SCALE_EPSILON = 0.0001
local DISPLAY_SCALE_PRESETS = { 0.0, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0 }
local MAXIMUM_RENDER_SCALE_PRESETS = { 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 0.0 }
local DEFAULT_MAIN_ITEMS = {
    { "script", "Scripts/Entry.lua" }, { "language", DEFAULT_LANGUAGE }, { "framerate", "30" },
    { "maxrenderscale", "2.0" }, { "antialiasinglevel", tostring(System.getAntiAliasingLevel()) },
    { "lightingrenderscale", "1.0" }, { "verticalsync", "true" }, { "musicon", "true" }, { "soundon", "true" },
    { "voiceon", "true" }, { "musicvolume", "100.00" }, { "soundvolume", "100.00" }, { "voicevolume", "100.00" }
}

MainConfig.SupportedLanguages = { "en_GB", "zh_CN" }

local function isSupportedLanguage(language)
    return table.contains(MainConfig.SupportedLanguages, language)
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

local function getDefaultDisplayScale()
    if PLATFORM == "ohos" and LUDORK_MOBILE and System.isDisplayScaleConfigurable() then
        return 0.0
    end
    return DEFAULT_DISPLAY_SCALE
end

local function createMainIni(iniFilePath, iniFile)
    iniFile:add_section("Main")
    for _, item in ipairs(DEFAULT_MAIN_ITEMS) do
        iniFile:set("Main", item[1], item[2])
    end
    iniFile:set("Main", "language", getInitialLanguage())
    iniFile:set("Main", "scale", tostring(getDefaultDisplayScale()))
    iniFile:write(iniFilePath)
end

local function scaleFits(scale, maximumScale)
    return maximumScale == nil or scale <= maximumScale + DISPLAY_SCALE_EPSILON
end

local function normalizeConfiguredScale(configuredScale)
    if configuredScale == nil or configuredScale ~= configuredScale
        or configuredScale < 0.0 or configuredScale == math.huge
        or configuredScale == -math.huge then
        return DEFAULT_DISPLAY_SCALE
    end
    return configuredScale
end

function MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and (maximumScale ~= maximumScale or maximumScale == math.huge or maximumScale == -math.huge) then
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

local function normalizeMaximumRenderScale(configuredScale)
    if configuredScale == nil or configuredScale ~= configuredScale
        or configuredScale < 0.0 or configuredScale == math.huge
        or configuredScale == -math.huge then
        return DEFAULT_MAXIMUM_RENDER_SCALE
    end
    return configuredScale
end

function MainConfig.GetMaximumRenderScaleOptions(configuredScale)
    configuredScale = normalizeMaximumRenderScale(configuredScale)
    local values = copy(MAXIMUM_RENDER_SCALE_PRESETS)
    if table.contains(values, configuredScale) then
        return values, configuredScale
    end
    local insertIndex = #values
    for index = 1, #values - 1 do
        if configuredScale < values[index] then
            insertIndex = index
            break
        end
    end
    table.insert(values, insertIndex, configuredScale)
    return values, configuredScale
end

function MainConfig.LoadOrCreate()
    local iniFilePath = getIniFilePath()
    local iniFile = configparser.ConfigParser()
    if CoreSystem.exists(iniFilePath) then
        iniFile:read(iniFilePath)
        Logging.debug("Loaded main configuration: %s", iniFilePath)
    else
        createMainIni(iniFilePath, iniFile)
        Logging.info("Created main configuration: %s", iniFilePath)
    end
    if iniFile:get("Main", "scale", nil) == nil then
        iniFile:set("Main", "scale", tostring(getDefaultDisplayScale()))
    end
    return iniFilePath, iniFile
end

return MainConfig
