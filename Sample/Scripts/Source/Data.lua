local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local DataLoading = require("Source.Data.Loading")
local DataTextConfigs = require("Source.Data.TextConfigs")
local DataBlueprints = require("Source.Data.Blueprints")

local GeneralDataKey = GeneralEnum.GeneralDataKey

local Data = {
    dataKinds = 7,
    ---@type table<string, Engine.AnimationData>
    _animationData = {},
    ---@type table<string, Engine.Curve|Engine.Vector2Curve|Engine.Vector3Curve|Engine.Vector4Curve>
    _curveData = {},
    ---@type table<string, string>
    _curveTypes = {},
    ---@type table<string, table>
    _textConfigData = {},
    ---@type table<string, Engine.PlainTextConfig>
    _plainTextConfigs = {},
    ---@type table<string, Engine.RichTextConfig>
    _richTextConfigs = {},
    ---@type table<string, Source.Data.GraphData>
    _commonFunctionsData = {},
    ---@type table<string, Engine.Tileset>
    _tilesetData = {},
    ---@type table<string, Engine.AutoTile>
    _autoTileData = {},
    ---@type table<string, table<string, Source.Data.JsonValue>>
    _generalData = {},
    ---@type string[] | nil
    _blueprintClassPaths = nil,
    ---@type table<string, string> | nil
    _blueprintClassPathIndex = nil,
    ---@type table<string, string|table>
    _blueprintClassData = {},
    _classDict = Engine.ClassDict.new()
}

local dataLoading = DataLoading.new(Data)
local dataTextConfigs = DataTextConfigs.new(Data)
local dataBlueprints = DataBlueprints.new(Data, dataLoading)

---@generic T
---@param values  table<string, T>
---@param key     string
---@param message string
---@return T
local function requireNamedValue(values, key, message)
    local value = rawget(values, key)
    assert(value ~= nil, message)
    return value
end

function Data.beginInitialLoad()
    return dataLoading:beginInitialLoad()
end

function Data.applyInitialLoadItem(stage, item)
    return dataLoading:applyInitialLoadItem(stage, item)
end

function Data.commitInitialLoad(stage)
    dataLoading:commitInitialLoad(stage, dataBlueprints)
end

function Data.abortInitialLoad(stage)
    dataLoading:abortInitialLoad(stage)
end

function Data.getDataKinds()
    return Data.dataKinds
end

function Data.countLoadableFiles(dataRoot, needExt, _defaultType, recursive)
    return dataLoading:countLoadableFiles(dataRoot, needExt, recursive)
end

function Data.loadAnimations(onFileLoaded)
    dataLoading:loadAnimations(onFileLoaded)
end

function Data.loadCommonFunctions(onFileLoaded)
    dataLoading:loadCommonFunctions(onFileLoaded)
end

function Data.loadTilesets(onFileLoaded)
    dataLoading:loadTilesets(onFileLoaded)
end

function Data.loadAutoTiles(onFileLoaded)
    dataLoading:loadAutoTiles(onFileLoaded)
end

function Data.loadGeneralData(onFileLoaded)
    dataLoading:loadGeneralData(onFileLoaded)
end

function Data.loadCurves(onFileLoaded)
    dataLoading:loadCurves(onFileLoaded)
end

function Data.loadTextConfigs(onFileLoaded)
    dataLoading:loadTextConfigs(onFileLoaded)
end

function Data.splitCompound(fileName)
    return dataLoading:splitCompound(fileName)
end

function Data.getAnimation(name)
    local animation = requireNamedValue(
        Data._animationData, name, "Animation data not found: " .. tostring(name)
    )
    return copy(animation)
end

function Data.getCurve(name)
    assert(Data._curveTypes[name] == "curve", "Float curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Float curve data not found: " .. tostring(name))
end

function Data.getVector2Curve(name)
    assert(Data._curveTypes[name] == "vector2Curve", "Vector2 curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Vector2 curve data not found: " .. tostring(name))
end

function Data.getVector3Curve(name)
    assert(Data._curveTypes[name] == "vector3Curve", "Vector3 curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Vector3 curve data not found: " .. tostring(name))
end

function Data.getVector4Curve(name)
    assert(Data._curveTypes[name] == "vector4Curve", "Vector4 curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Vector4 curve data not found: " .. tostring(name))
end

function Data.getPlainTextConfig(name)
    return dataTextConfigs:getPlainTextConfig(name)
end

function Data.getRichTextConfig(name)
    return dataTextConfigs:getRichTextConfig(name)
end

function Data.getTileset(name)
    return requireNamedValue(Data._tilesetData, name, "Tileset data not found: " .. tostring(name))
end

function Data.getAutoTile(name)
    return requireNamedValue(Data._autoTileData, name, "AutoTile data not found: " .. tostring(name))
end

function Data.hasAutoTile(name)
    return Data._autoTileData[name] ~= nil
end

function Data.getGeneralData(name)
    return requireNamedValue(Data._generalData, name, "General data not found: " .. tostring(name))
end

---@param name string
---@return table<string, table<string, Source.Data.JsonValue>>
local function generalMembers(name)
    local data = Data.getGeneralData(name)
    local members = data.members
    if members == nil then
        return {}
    end
    ---@cast members table<string, table<string, Source.Data.JsonValue>>
    return members
end

function Data.getGeneralClassData(key)
    return generalMembers(GeneralDataKey.Class)[key] or {}
end

function Data.getGeneralEnemyData(key)
    return generalMembers(GeneralDataKey.Enemy)[key] or {}
end

function Data.getGeneralPlayerData(key)
    return generalMembers(GeneralDataKey.Player)[key] or {}
end

function Data.getAllGeneralEquipData()
    return generalMembers(GeneralDataKey.Equip)
end

function Data.getGeneralEquipData(key)
    return generalMembers(GeneralDataKey.Equip)[key] or {}
end

function Data.getAllGeneralItemData()
    return generalMembers(GeneralDataKey.Item)
end

function Data.getGeneralItemData(key)
    return generalMembers(GeneralDataKey.Item)[key] or {}
end

function Data.getGeneralSpecialData(key)
    return generalMembers(GeneralDataKey.Special)[key] or {}
end

function Data.getGeneralStateData(key)
    return generalMembers(GeneralDataKey.State)[key] or {}
end

function Data.getClass(classPath)
    return Data._classDict:get(classPath)
end

function Data.getClassData(classPath)
    return Data._classDict:getData(classPath)
end

function Data.resolveClassPath(className)
    return dataBlueprints:resolveClassPath(className)
end

function Data.getCommonFunction(name)
    return dataBlueprints:getCommonFunction(name)
end

function Data.genGraphFromData(data, parent, parentClass)
    return dataBlueprints:genGraphFromData(data, parent, parentClass)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function Data.genActorFromClassPath(classPath, tag, classVarChanges)
    return dataBlueprints:genActorFromClassPath(classPath, tag, classVarChanges)
end

function Data.genActorFromClassName(className, tag)
    return dataBlueprints:genActorFromClassName(className, tag)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function Data.genActorFromData(actorData, layerName, classVarChanges)
    return dataBlueprints:genActorFromData(actorData, layerName, classVarChanges)
end

Class.registerService("curve", function (name)
    return Data.getCurve(name)
end)

Class.registerService("vector2Curve", function (name)
    return Data.getVector2Curve(name)
end)

Class.registerService("vector3Curve", function (name)
    return Data.getVector3Curve(name)
end)

Class.registerService("vector4Curve", function (name)
    return Data.getVector4Curve(name)
end)

Class.registerService("plainTextConfig", function (name)
    return Data.getPlainTextConfig(name)
end)

Class.registerService("richTextConfig", function (name)
    return Data.getRichTextConfig(name)
end)

dataBlueprints:registerServices()

return Data
