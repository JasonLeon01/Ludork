local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameMap = require("Global.GameMap")
local MainConfig = require("Source.Configs.Main")

local Display = GlobalCore.Display
local ResourceFileConstants = Engine.ResourceFileConstants
local FontManager = GlobalCore.FontManager
local RuntimeProviders = Engine.RuntimeProviders

---@type function
local extractConfigValues
local System = {}

local systemState = {
    title = "",
    fonts = {},
    fontSize = 32,
    windowskinName = "",
    titleBackgroundFile = "/Game/Assets/System/GrassBackground.png",
    coverOpaqueAlpha = 0,
    startMap = "",
    startPlayerClassPath = "",
    startRegion = "",
    startPos = sf.Vector2u.new(0, 0),
    cursorSE = "",
    decisionSE = "",
    cancelSE = "",
    buzzerSE = "",
    shopSE = "",
    saveSE = "",
    loadSE = "",
    gateSE = "",
    stairSE = "",
    getSE = "",
    equipSE = "",
    titleBGM = "",
    audioConfigValues = {},
    savedScreenImage = nil
}

---@param relativePath string
---@return string
local function blueprintRelativePathToClassPath(relativePath)
    assert(Class.isInstance(relativePath, "string"), "Start player blueprint path must be a string")
    assert(
        string.endsWith(relativePath, ResourceFileConstants.DATA_EXTENSION),
        "Start player blueprint path must end with .json"
    )
    assert(relativePath:sub(1, 1) ~= "/", "Start player blueprint path must be relative")
    assert(not relativePath:find("\\", 1, true), "Start player blueprint path must use / separators")
    assert(not relativePath:find("//", 1, true), "Start player blueprint path contains an empty segment")
    local classRelativePath = relativePath:sub(1, -6)
    assert(bool(classRelativePath), "Start player blueprint path must not be empty")
    assert(classRelativePath:sub(-1) ~= "/", "Start player blueprint path contains an empty segment")
    for segment in classRelativePath:gmatch("[^/]+") do
        assert(segment ~= "." and segment ~= "..", "Start player blueprint path contains an invalid segment")
    end
    return "Data.Blueprints." .. classRelativePath:gsub("/", ".")
end

function System.Init()
    local systemData = Engine.getJSONData("./Data/Configs/System.json")
    assert(Class.isInstance(systemData, "table"), "System configuration must be an object")
    ---@cast systemData Source.System.ConfigData
    systemState.title = systemData.title.value
    local size = systemData.gameSize.value
    local gameSize = sf.Vector2u.new(size[1], size[2])
    ---@cast gameSize sf.Vector2u
    systemState.fonts = {}
    for _, font in ipairs(systemData.fonts.value) do
        systemState.fonts[#systemState.fonts + 1] = FontManager.load(font)
    end
    systemState.fontSize = systemData.fontSize.value
    local iconPath = systemData.icon.value
    local cursorPath = systemData.cursor.value
    systemState.windowskinName = systemData.windowskinName.value
    systemState.titleBackgroundFile = systemData.titleBackgroundFile.value
    local coverOpaqueAlpha = systemData.coverOpaqueAlpha.value
    systemState.coverOpaqueAlpha = coverOpaqueAlpha
    systemState.startMap = systemData.startMap.value
    systemState.startPlayerClassPath = blueprintRelativePathToClassPath(systemData.startPlayerBlueprint.value)
    systemState.startRegion = systemData.startRegion.value
    assert(
        Class.isInstance(systemState.startRegion, "string") and bool(systemState.startRegion),
        "Start region must be a non-empty string"
    )
    local startPos = systemData.startPos.value
    systemState.startPos = sf.Vector2u.new(startPos[1], startPos[2])
    local configuredScale = Display.getConfiguredScale()
    local maximumScale = Display.getMaximumWindowedScale(gameSize)
    local _, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and effectiveScale ~= configuredScale then
        Display.setScale(effectiveScale)
    end
    Display.initializeDisplay(systemState.title, gameSize, iconPath, cursorPath)
    Engine.DefaultFont = systemState.fonts[1]
    Engine.DefaultFontSize = systemState.fontSize
    Engine.DefaultWindowskinName = systemState.windowskinName
    GameMap.DefaultCoverAlpha = coverOpaqueAlpha
    local audioData = Engine.getJSONData("./Data/Configs/Audio.json")
    systemState.audioConfigValues = extractConfigValues(audioData)
    systemState.cursorSE = tostring(systemState.audioConfigValues.cursorSE or "")
    systemState.decisionSE = tostring(systemState.audioConfigValues.decisionSE or "")
    systemState.cancelSE = tostring(systemState.audioConfigValues.cancelSE or "")
    systemState.buzzerSE = tostring(systemState.audioConfigValues.buzzerSE or "")
    systemState.shopSE = tostring(systemState.audioConfigValues.shopSE or "")
    systemState.saveSE = tostring(systemState.audioConfigValues.saveSE or "")
    systemState.loadSE = tostring(systemState.audioConfigValues.loadSE or "")
    systemState.gateSE = tostring(systemState.audioConfigValues.gateSE or "")
    systemState.stairSE = tostring(systemState.audioConfigValues.stairSE or "")
    systemState.getSE = tostring(systemState.audioConfigValues.getSE or "")
    systemState.equipSE = tostring(systemState.audioConfigValues.equipSE or "")
    systemState.titleBGM = tostring(systemState.audioConfigValues.titleBGM or "")
end

---@param configData table<string, string | { value: string }>
---@return table<string, string>
function extractConfigValues(configData)
    local result = {}
    for key, setting in pairs(configData) do
        if Class.isInstance(key, "string") and Class.isInstance(setting, "table") and setting.value ~= nil then
            result[key] = setting.value
        end
    end
    return result
end

function System.GetConfigValue(configName, settingName)
    if configName == "Audio" then
        return systemState.audioConfigValues[settingName] or ""
    end
    return ""
end

function System.InstallRuntimeProviders()
    RuntimeProviders.installConfig(function (configName, settingName)
        local value = System.GetConfigValue(configName, settingName)
        return Class.isInstance(value, "string") and value or tostring(value)
    end)
end

function System.GetTitle()
    return systemState.title
end

function System.GetFonts()
    return systemState.fonts
end

function System.GetFontSize()
    return systemState.fontSize
end

function System.GetWindowskinName()
    return systemState.windowskinName
end

function System.GetTitleBackgroundFile()
    return systemState.titleBackgroundFile
end

function System.SetWindowskinName(name)
    systemState.windowskinName = name
end

function System.GetStartMap()
    return systemState.startMap
end

function System.GetStartPlayerClassPath()
    return systemState.startPlayerClassPath
end

function System.GetStartRegion()
    return systemState.startRegion
end

function System.GetStartPos()
    return systemState.startPos
end

function System.GetCursorSE()
    return systemState.cursorSE
end

function System.GetDecisionSE()
    return systemState.decisionSE
end

function System.GetCancelSE()
    return systemState.cancelSE
end

function System.GetBuzzerSE()
    return systemState.buzzerSE
end

function System.GetShopSE()
    return systemState.shopSE
end

function System.GetSaveSE()
    return systemState.saveSE
end

function System.GetLoadSE()
    return systemState.loadSE
end

function System.GetGateSE()
    return systemState.gateSE
end

function System.GetStairSE()
    return systemState.stairSE
end

function System.GetGetSE()
    return systemState.getSE
end

function System.GetEquipSE()
    return systemState.equipSE
end

function System.GetTitleBGM()
    return systemState.titleBGM
end

function System.GetSavedScreenImage()
    return systemState.savedScreenImage
end

function System.SetSavedScreenImage(image)
    systemState.savedScreenImage = image
end

return System
