local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local Locale = require("Source.Locale.Core")

local MainConfig = {}
local DEFAULT_LANGUAGE = "en_GB"
local DEFAULT_MAIN_ITEMS = {
    { "script", "Scripts/Entry.lua" }, { "language", DEFAULT_LANGUAGE }, { "scale", "2.0" }, { "framerate", "120" },
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
