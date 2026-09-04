local Engine = require("Engine")
local Logging = require("Global.Utils.Logging")
local EventKeys = require("Source.Configs.EventKeys")

local Core = {}
local LOCALE_MODULE_PREFIX = "Source.Locale."
local dataDict = {}

Core.LANGUAGE = "en_GB"

function Core.GetLocaleKeys()
    return table.orderedStringKeys(dataDict)
end

function Core.Init()
    dataDict = {}
    local languages = {}
    for moduleName in pairs(package.preload) do
        if Class.isInstance(moduleName, "string") then
            local language = moduleName:match("^Source%.Locale%.([^.]+)$")
            if language ~= nil and language ~= "Core" then
                languages[#languages + 1] = language
            end
        end
    end
    table.sort(languages)
    for _, language in ipairs(languages) do
        dataDict[language] = require(LOCALE_MODULE_PREFIX .. language)
    end
end

function Core.GetLocaleContent(localeKey, key)
    local data = dataDict[localeKey]
    if data == nil then
        return key
    end
    return data[key] or key
end

function Core.GetContent(key)
    if dataDict[Core.LANGUAGE] ~= nil then
        return Core.GetLocaleContent(Core.LANGUAGE, key)
    end
    return Core.GetLocaleContent("en_GB", key)
end

function Core.GetLocaleDict()
    if dataDict[Core.LANGUAGE] ~= nil then
        return dataDict[Core.LANGUAGE]
    end
    return dataDict.en_GB or {}
end

function Core.ApplyStringLocaleFormat(value)
    if not Class.isInstance(value, "string") then
        return value
    end
    local data = Core.GetLocaleDict()
    if data[value] ~= nil then
        return data[value]
    end
    if not bool(data) then
        return value
    end
    local result = {}
    local position = 1
    while position <= #value do
        local open = value:find("{", position, true)
        if open == nil then
            result[#result + 1] = value:sub(position)
            break
        end
        result[#result + 1] = value:sub(position, open - 1)
        if value:sub(open + 1, open + 1) == "{" then
            result[#result + 1] = "{"
            position = open + 2
        else
            local close = value:find("}", open + 1, true)
            if close == nil then
                result[#result + 1] = value:sub(open)
                break
            end
            local key = value:sub(open + 1, close - 1)
            local replacement = data[key]
            result[#result + 1] = replacement ~= nil and tostring(replacement) or value:sub(open, close)
            position = close + 1
        end
    end
    return (table.concat(result):gsub("}}", "}"))
end

function Core.SetLanguage(language)
    local resolved = bool(language) and language or "en_GB"
    if resolved == Core.LANGUAGE then
        return
    end
    Core.LANGUAGE = resolved
    Logging.info("Language changed to %s", resolved)
    Engine.publish(EventKeys.LocaleChanged, {
        language = resolved
    })
end

function Core.GetLanguage()
    return Core.LANGUAGE
end

function Core.HasLanguage(language)
    return dataDict[language] ~= nil
end

function Core.HasKey(key)
    return Core.GetLocaleDict()[key] ~= nil
end

function Core.ResolveLanguage(language)
    local resolved = tostring(language or "")
    if not bool(resolved) then
        resolved = Core.LANGUAGE
    end
    resolved = resolved:gsub("%..*$", ""):gsub("@.*$", ""):gsub("-", "_")
    local normalized = resolved:lower()
    local languageCode = normalized:match("^([^_]+)")
    local languageMatch = nil
    local ambiguous = false
    for loadedLanguage in pairs(dataDict) do
        local loadedNormalized = loadedLanguage:lower()
        if loadedNormalized == normalized then
            return loadedLanguage
        end
        if loadedNormalized:match("^([^_]+)") == languageCode then
            if languageMatch == nil then
                languageMatch = loadedLanguage
            else
                ambiguous = true
            end
        end
    end
    if languageMatch ~= nil and not ambiguous then
        return languageMatch
    end
    return "en_GB"
end

Core.LOC = Core.ApplyStringLocaleFormat
Core.LOC_L = Core.GetLocaleContent
Core.LOC_D = Core.GetLocaleDict

Core.Init()

return Core
