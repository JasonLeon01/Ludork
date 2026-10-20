local Engine = require("Engine")
local Logging = require("Global.Utils.Logging")
local Locale = require("Source.Locale.Core")

local MainConfig = {}
local DEFAULT_DISPLAY_SCALE = 0.0
local INVALID_DISPLAY_SCALE_FALLBACK = 1.0
local INVALID_MAXIMUM_RENDER_SCALE_FALLBACK = 2.0
local DISPLAY_SCALE_EPSILON = 0.0001
local DISPLAY_SCALE_PRESETS = { 0.0, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0 }
local MAXIMUM_RENDER_SCALE_PRESETS = { 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 0.0 }
local DEFAULT_MAIN_ITEMS = {
    { "language", Locale.DEFAULT_LANGUAGE }, { "framerate", "60" }, { "maxrenderscale", "2.0" },
    { "antialiasinglevel", "8" }, { "lightingrenderscale", "1.0" }, { "verticalsync", "true" }, { "musicon", "true" },
    { "soundon", "true" }, { "voiceon", "true" }, { "musicvolume", "100.00" }, { "soundvolume", "100.00" },
    { "voicevolume", "100.00" }
}

MainConfig.SupportedLanguages = { Locale.DEFAULT_LANGUAGE, "zh_CN" }

local function isSupportedLanguage(language)
    return table.contains(MainConfig.SupportedLanguages, language)
end

local function getInitialLanguage()
    local language = Locale.ResolveLanguage(locale.getdefaultlocale())
    if isSupportedLanguage(language) then
        return language
    end
    return Locale.DEFAULT_LANGUAGE
end

local function createMainConfig(configFilePath, configFile)
    configFile:add_section("Main")
    for _, item in ipairs(DEFAULT_MAIN_ITEMS) do
        configFile:set("Main", item[1], item[2])
    end
    configFile:set("Main", "language", getInitialLanguage())
    configFile:set("Main", "scale", tostring(DEFAULT_DISPLAY_SCALE))
    configFile:write(configFilePath)
end

local function scaleFits(scale, maximumScale)
    return maximumScale == nil or scale <= maximumScale + DISPLAY_SCALE_EPSILON
end

local function normalizeConfiguredScale(configuredScale)
    if configuredScale == nil then
        return DEFAULT_DISPLAY_SCALE
    end
    if not math.isFinite(configuredScale) or configuredScale < 0.0 then
        return INVALID_DISPLAY_SCALE_FALLBACK
    end
    return configuredScale
end

function MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and not math.isFinite(maximumScale) then
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
    if configuredScale == nil or not math.isFinite(configuredScale) or configuredScale < 0.0 then
        return INVALID_MAXIMUM_RENDER_SCALE_FALLBACK
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
    local configFilePath = Engine.getMainConfigPath()
    local configFile = configparser.ConfigParser()
    if os.path.isfile(configFilePath) then
        configFile:read(configFilePath)
        Logging.debug("Loaded main configuration: %s", configFilePath)
    else
        createMainConfig(configFilePath, configFile)
        Logging.info("Created main configuration: %s", configFilePath)
    end
    if configFile:get("Main", "scale", nil) == nil then
        configFile:set("Main", "scale", tostring(DEFAULT_DISPLAY_SCALE))
    end
    return configFilePath, configFile
end

return MainConfig
