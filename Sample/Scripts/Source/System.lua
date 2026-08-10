local Engine = require("Engine")
local GameMap = require("Global.GameMap")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

local System = {}

System._title = ""
System._fonts = {}
System._fontSize = 32
System._windowskinName = ""
System._titleBackgroundFile = "GrassBackground.png"
System._coverOpaqueAlpha = 0
System._startMap = ""
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

function System.init()
    local MovementSpecials = require("Source.MovementSpecials")

    local systemData = Engine.getJSONData("./Data/Configs/System.json")
    System._title = systemData.title.value
    local size = systemData.gameSize.value
    local gameSize = sf.Vector2u.new(size[1], size[2])
    System._fonts = {}
    for _, font in ipairs(systemData.fonts.value) do
        System._fonts[#System._fonts + 1] = ManagerFunctions.loadFont(font)
    end
    System._fontSize = systemData.fontSize.value
    local iconPath = os.path.join("./Assets", systemData.icon.base, systemData.icon.value)
    local cursorPath = os.path.join("./Assets", systemData.cursor.base, systemData.cursor.value)
    System._windowskinName = systemData.windowskinName.value
    System._titleBackgroundFile = systemData.titleBackgroundFile.value
    Engine.CellSize = systemData.cellSize.value
    local coverOpaqueAlpha = systemData.coverOpaqueAlpha.value
    System._coverOpaqueAlpha = coverOpaqueAlpha
    System._startMap = systemData.startMap.value
    local startPos = systemData.startPos.value
    System._startPos = sf.Vector2u.new(startPos[1], startPos[2])
    GlobalSystem.initializeDisplay(System._title, gameSize, iconPath, cursorPath)
    Engine.DefaultFont = System._fonts[1]
    Engine.DefaultFontSize = System._fontSize
    Engine.DefaultWindowskinName = System._windowskinName
    GameMap.DefaultCoverAlpha = coverOpaqueAlpha
    local audioData = Engine.getJSONData("./Data/Configs/Audio.json")
    System._audioConfigValues = System._extractConfigValues(audioData)
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
    MovementSpecials.registerHandlers()
end

---@param configData table<string, string | { value: string }>
---@return table<string, string>
function System._extractConfigValues(configData)
    local result = {}
    for key, setting in pairs(configData) do
        if type(key) == "string" and type(setting) == "table" and setting.value ~= nil then
            result[key] = setting.value
        end
    end
    return result
end

function System.getConfigValue(configName, settingName)
    if configName == "Audio" then
        return System._audioConfigValues[settingName] or ""
    end
    return ""
end

Class.registerService("config.resolve", function (configName, settingName)
    local value = System.getConfigValue(configName, settingName)
    return type(value) == "string" and value or tostring(value)
end)

function System.getTitle()
    return System._title
end

function System.getFonts()
    return System._fonts
end

function System.getFontSize()
    return System._fontSize
end

function System.getWindowskinName()
    return System._windowskinName
end

function System.getTitleBackgroundFile()
    return System._titleBackgroundFile
end

function System.setWindowskinName(name)
    System._windowskinName = name
end

function System.getStartMap()
    return System._startMap
end

function System.getStartPos()
    return System._startPos
end

function System.getCursorSE()
    return System._cursorSE
end

function System.getDecisionSE()
    return System._decisionSE
end

function System.getCancelSE()
    return System._cancelSE
end

function System.getBuzzerSE()
    return System._buzzerSE
end

function System.getShopSE()
    return System._shopSE
end

function System.getSaveSE()
    return System._saveSE
end

function System.getLoadSE()
    return System._loadSE
end

function System.getGateSE()
    return System._gateSE
end

function System.getStairSE()
    return System._stairSE
end

function System.getGetSE()
    return System._getSE
end

function System.getEquipSE()
    return System._equipSE
end

function System.getTitleBGM()
    return System._titleBGM
end

function System.getSavedScreenImage()
    return System._savedScreenImage
end

function System.setSavedScreenImage(image)
    System._savedScreenImage = image
end

return System
