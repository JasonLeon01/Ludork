local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local DataLoading = require("Source.Data.Loading")
local DataTextConfigs = require("Source.Data.TextConfigs")
local DataBlueprints = require("Source.Data.Blueprints")
local Validation = require("Source.Data.Validation")

local GeneralDataKey = GeneralEnum.GeneralDataKey
local requireNamedValue = Validation.RequireNamedValue

---@class (partial) Source.Data
local Data = {
    dataKinds = 7,
    _animationData = {},
    _curveData = {},
    _curveTypes = {},
    _textConfigData = {},
    _plainTextConfigs = {},
    _richTextConfigs = {},
    _commonFunctionsData = {},
    _tilesetData = {},
    _autoTileData = {},
    _generalData = {},
    _blueprintClassPaths = nil,
    _blueprintClassPathIndex = nil,
    _blueprintClassData = {},
    _classDict = Engine.ClassDict.new()
}

local dataLoading = DataLoading.new(Data)
local dataTextConfigs = DataTextConfigs.new(Data)
local dataBlueprints = DataBlueprints.new(Data, dataLoading)

function Data.BeginInitialLoad()
    return dataLoading:beginInitialLoad()
end

function Data.ApplyInitialLoadItem(stage, item)
    return dataLoading:applyInitialLoadItem(stage, item)
end

function Data.CommitInitialLoad(stage)
    dataLoading:commitInitialLoad(stage, dataBlueprints)
end

function Data.AbortInitialLoad(stage)
    dataLoading:abortInitialLoad(stage)
end

function Data.GetDataKinds()
    return Data.dataKinds
end

function Data.CountLoadableFiles(dataRoot, needExt, _defaultType, recursive)
    return dataLoading:countLoadableFiles(dataRoot, needExt, recursive)
end

function Data.LoadAnimations(onFileLoaded)
    dataLoading:loadAnimations(onFileLoaded)
end

function Data.LoadCommonFunctions(onFileLoaded)
    dataLoading:loadCommonFunctions(onFileLoaded)
end

function Data.LoadTilesets(onFileLoaded)
    dataLoading:loadTilesets(onFileLoaded)
end

function Data.LoadAutoTiles(onFileLoaded)
    dataLoading:loadAutoTiles(onFileLoaded)
end

function Data.LoadGeneralData(onFileLoaded)
    dataLoading:loadGeneralData(onFileLoaded)
end

function Data.LoadCurves(onFileLoaded)
    dataLoading:loadCurves(onFileLoaded)
end

function Data.LoadTextConfigs(onFileLoaded)
    dataLoading:loadTextConfigs(onFileLoaded)
end

function Data.GetAnimation(name)
    local animation = requireNamedValue(Data._animationData, name, "Animation data not found: " .. tostring(name))
    return copy(animation)
end

function Data.GetCurve(name)
    assert(Data._curveTypes[name] == "curve", "Float curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Float curve data not found: " .. tostring(name))
end

function Data.GetVector2Curve(name)
    assert(Data._curveTypes[name] == "vector2Curve", "Vector2 curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Vector2 curve data not found: " .. tostring(name))
end

function Data.GetVector3Curve(name)
    assert(Data._curveTypes[name] == "vector3Curve", "Vector3 curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Vector3 curve data not found: " .. tostring(name))
end

function Data.GetVector4Curve(name)
    assert(Data._curveTypes[name] == "vector4Curve", "Vector4 curve data not found: " .. tostring(name))
    return requireNamedValue(Data._curveData, name, "Vector4 curve data not found: " .. tostring(name))
end

function Data.GetPlainTextConfig(name)
    return dataTextConfigs:getPlainTextConfig(name)
end

function Data.GetRichTextConfig(name)
    return dataTextConfigs:getRichTextConfig(name)
end

function Data.GetTileset(name)
    return requireNamedValue(Data._tilesetData, name, "Tileset data not found: " .. tostring(name))
end

function Data.GetAutoTile(name)
    return requireNamedValue(Data._autoTileData, name, "AutoTile data not found: " .. tostring(name))
end

function Data.HasAutoTile(name)
    return Data._autoTileData[name] ~= nil
end

function Data.GetGeneralData(name)
    return requireNamedValue(Data._generalData, name, "General data not found: " .. tostring(name))
end

---@param name string
---@return table<string, table<string, Source.Data.GeneralValue>>
local function generalMembers(name)
    local data = Data.GetGeneralData(name)
    local members = requireNamedValue(data, "members", "General data members not found: " .. tostring(name))
    ---@cast members table<string, table<string, Source.Data.GeneralValue>>
    return members
end

function Data.GetGeneralClassData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Class), key, "General class data not found: " .. tostring(key)
    )
end

function Data.GetGeneralEnemyData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Enemy), key, "General enemy data not found: " .. tostring(key)
    )
end

function Data.GetGeneralPlayerData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Player), key, "General player data not found: " .. tostring(key)
    )
end

function Data.GetAllGeneralEquipData()
    return generalMembers(GeneralDataKey.Equip)
end

function Data.GetGeneralEquipData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Equip), key, "General equip data not found: " .. tostring(key)
    )
end

function Data.GetAllGeneralItemData()
    return generalMembers(GeneralDataKey.Item)
end

function Data.GetGeneralItemData(key)
    return requireNamedValue(generalMembers(GeneralDataKey.Item), key, "General item data not found: " .. tostring(key))
end

function Data.GetGeneralSpecialData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Special), key, "General special data not found: " .. tostring(key)
    )
end

function Data.GetGeneralStateData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.State), key, "General state data not found: " .. tostring(key)
    )
end

function Data.GetClass(classPath)
    return Data._classDict:get(classPath)
end

function Data.GetClassData(classPath)
    return Data._classDict:getData(classPath)
end

function Data.ResolveClassPath(className)
    return dataBlueprints:resolveClassPath(className)
end

function Data.GetCommonFunction(name)
    return dataBlueprints:getCommonFunction(name)
end

function Data.GenGraphFromData(data, parent, parentClass)
    return dataBlueprints:genGraphFromData(data, parent, parentClass)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function Data.GenActorFromClassPath(classPath, tag, classVarChanges)
    return dataBlueprints:genActorFromClassPath(classPath, tag, classVarChanges)
end

function Data.GenActorFromClassName(className, tag)
    return dataBlueprints:genActorFromClassName(className, tag)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function Data.GenActorFromData(actorData, layerName, classVarChanges)
    return dataBlueprints:genActorFromData(actorData, layerName, classVarChanges)
end

Class.registerService("curve", function (name)
    return Data.GetCurve(name)
end)

Class.registerService("vector2Curve", function (name)
    return Data.GetVector2Curve(name)
end)

Class.registerService("vector3Curve", function (name)
    return Data.GetVector3Curve(name)
end)

Class.registerService("vector4Curve", function (name)
    return Data.GetVector4Curve(name)
end)

Class.registerService("plainTextConfig", function (name)
    return Data.GetPlainTextConfig(name)
end)

Class.registerService("richTextConfig", function (name)
    return Data.GetRichTextConfig(name)
end)

dataBlueprints:registerServices()

return Data
