local Engine = require("Engine")
local GameMap = require("Global.GameMap")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local MainConfig = require("Source.Configs.Main")

local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

---@type function
local extractConfigValues
local System = {}

System._title = ""
System._fonts = {}
System._fontSize = 32
System._windowskinName = ""
System._titleBackgroundFile = "GrassBackground.png"
System._coverOpaqueAlpha = 0
System._startMap = ""
System._startPlayerClassPath = ""
System._startRegion = ""
System._startPos = sf.Vector2u.new(0, 0)
System._cursorSE = ""
System._decisionSE = ""
System._cancelSE = ""
System._buzzerSE = ""
System._shopSE = ""
System._saveSE = ""
System._loadSE = ""
System._gateSE = ""
System._stairSE = ""
System._getSE = ""
System._equipSE = ""
System._titleBGM = ""
System._audioConfigValues = {}
System._savedScreenImage = nil

---@param relativePath string
---@return string
local function blueprintRelativePathToClassPath(relativePath)
    assert(type(relativePath) == "string", "Start player blueprint path must be a string")
    assert(relativePath:sub(-5) == ".json", "Start player blueprint path must end with .json")
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
    local MovementSpecials = require("Source.MovementSpecials")

    local systemData = Engine.getJSONData("./Data/Configs/System.json")
    System._title = systemData.title.value
    local size = systemData.gameSize.value
    local gameSize = sf.Vector2u.new(size[1], size[2])
    ---@cast gameSize sf.Vector2u
    System._fonts = {}
    for _, font in ipairs(systemData.fonts.value) do
        System._fonts[#System._fonts + 1] = ManagerFunctions.loadFont(font)
    end
    System._fontSize = systemData.fontSize.value
    local iconPath = os.path.join("Assets", systemData.icon.base, systemData.icon.value)
    local cursorPath = os.path.join("Assets", systemData.cursor.base, systemData.cursor.value)
    System._windowskinName = systemData.windowskinName.value
    System._titleBackgroundFile = systemData.titleBackgroundFile.value
    Engine.CellSize = systemData.cellSize.value
    local coverOpaqueAlpha = systemData.coverOpaqueAlpha.value
    System._coverOpaqueAlpha = coverOpaqueAlpha
    System._startMap = systemData.startMap.value
    System._startPlayerClassPath = blueprintRelativePathToClassPath(systemData.startPlayerBlueprint.value)
    System._startRegion = systemData.startRegion.value
    assert(
        type(System._startRegion) == "string" and bool(System._startRegion), "Start region must be a non-empty string"
    )
    local startPos = systemData.startPos.value
    System._startPos = sf.Vector2u.new(startPos[1], startPos[2])
    local configuredScale = GlobalSystem.getConfiguredScale()
    local maximumScale = GlobalSystem.getMaximumWindowedScale(gameSize)
    local _, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and effectiveScale ~= configuredScale then
        GlobalSystem.setScale(effectiveScale)
    end
    GlobalSystem.initializeDisplay(System._title, gameSize, iconPath, cursorPath)
    Engine.DefaultFont = System._fonts[1]
    Engine.DefaultFontSize = System._fontSize
    Engine.DefaultWindowskinName = System._windowskinName
    GameMap.DefaultCoverAlpha = coverOpaqueAlpha
    local audioData = Engine.getJSONData("./Data/Configs/Audio.json")
    System._audioConfigValues = extractConfigValues(audioData)
    System._cursorSE = tostring(System._audioConfigValues.cursorSE or "")
    System._decisionSE = tostring(System._audioConfigValues.decisionSE or "")
    System._cancelSE = tostring(System._audioConfigValues.cancelSE or "")
    System._buzzerSE = tostring(System._audioConfigValues.buzzerSE or "")
    System._shopSE = tostring(System._audioConfigValues.shopSE or "")
    System._saveSE = tostring(System._audioConfigValues.saveSE or "")
    System._loadSE = tostring(System._audioConfigValues.loadSE or "")
    System._gateSE = tostring(System._audioConfigValues.gateSE or "")
    System._stairSE = tostring(System._audioConfigValues.stairSE or "")
    System._getSE = tostring(System._audioConfigValues.getSE or "")
    System._equipSE = tostring(System._audioConfigValues.equipSE or "")
    System._titleBGM = tostring(System._audioConfigValues.titleBGM or "")
    MovementSpecials.RegisterHandlers()
end

---@param configData table<string, string | { value: string }>
---@return table<string, string>
function extractConfigValues(configData)
    local result = {}
    for key, setting in pairs(configData) do
        if type(key) == "string" and type(setting) == "table" and setting.value ~= nil then
            result[key] = setting.value
        end
    end
    return result
end

function System.GetConfigValue(configName, settingName)
    if configName == "Audio" then
        return System._audioConfigValues[settingName] or ""
    end
    return ""
end

Class.registerService("config.resolve", function (configName, settingName)
    local value = System.GetConfigValue(configName, settingName)
    return type(value) == "string" and value or tostring(value)
end)

function System.GetTitle()
    return System._title
end

function System.GetFonts()
    return System._fonts
end

function System.GetFontSize()
    return System._fontSize
end

function System.GetWindowskinName()
    return System._windowskinName
end

function System.GetTitleBackgroundFile()
    return System._titleBackgroundFile
end

function System.SetWindowskinName(name)
    System._windowskinName = name
end

function System.GetStartMap()
    return System._startMap
end

function System.GetStartPlayerClassPath()
    return System._startPlayerClassPath
end

function System.GetStartRegion()
    return System._startRegion
end

function System.GetStartPos()
    return System._startPos
end

function System.GetCursorSE()
    return System._cursorSE
end

function System.GetDecisionSE()
    return System._decisionSE
end

function System.GetCancelSE()
    return System._cancelSE
end

function System.GetBuzzerSE()
    return System._buzzerSE
end

function System.GetShopSE()
    return System._shopSE
end

function System.GetSaveSE()
    return System._saveSE
end

function System.GetLoadSE()
    return System._loadSE
end

function System.GetGateSE()
    return System._gateSE
end

function System.GetStairSE()
    return System._stairSE
end

function System.GetGetSE()
    return System._getSE
end

function System.GetEquipSE()
    return System._equipSE
end

function System.GetTitleBGM()
    return System._titleBGM
end

function System.GetSavedScreenImage()
    return System._savedScreenImage
end

function System.SetSavedScreenImage(image)
    System._savedScreenImage = image
end

return System
