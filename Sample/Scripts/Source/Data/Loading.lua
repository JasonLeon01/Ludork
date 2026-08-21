local cjson = require("cjson")
local Engine = require("Engine")
local Logging = require("Global.Utils.Logging")
---@type Global.Utils.Path.Module
local Path = require("Global.Utils.Path")

local Curve = Engine.Curve
local Vector2Curve = Engine.Vector2Curve
local Vector3Curve = Engine.Vector3Curve
local Vector4Curve = Engine.Vector4Curve

---@type table<string, boolean>
local generalDataScalarTypes = {
    any = true,
    bool = true,
    file = true,
    float = true,
    int = true,
    string = true,
    ["sf.Color"] = true,
    ["sf.IntRect"] = true,
    ["sf.Vector2f"] = true,
    ["sf.Vector2i"] = true,
    ["sf.Vector2u"] = true,
    ["sf.Vector3f"] = true,
    ["sf.Vector3i"] = true,
    ["sf.Vector3u"] = true,
}

---@type table<string, boolean>
local generalDataTypes = deepcopy(generalDataScalarTypes)
generalDataTypes.list = true
generalDataTypes.dict = true

local categoryFields = {
    animations = "_animationData",
    commonFunctions = "_commonFunctionsData",
    tilesets = "_tilesetData",
    autoTiles = "_autoTileData",
    general = "_generalData",
    curves = "_curveData",
    textConfigs = "_textConfigData"
}

local DataLoading = {}

---@param relativePath string
---@param context      string
---@param message      string
local function generalDataError(relativePath, context, message)
    error(string.format("Invalid General Data %s in %s: %s", context, relativePath, message))
end

---@param value any
---@return boolean
local function isArray(value)
    if type(value) ~= "table" then
        return false
    end
    local count = 0
    local maximum = 0
    for key in pairs(value) do
        if type(key) ~= "number" or math.type(key) ~= "integer" or key < 1 then
            return false
        end
        count = count + 1
        maximum = math.max(maximum, key)
    end
    return count == maximum
end

---@param value any
---@return boolean
local function isDictionary(value)
    if type(value) ~= "table" then
        return false
    end
    for key in pairs(value) do
        if type(key) ~= "string" then
            return false
        end
    end
    return true
end

---@param value        any
---@param typeName     string
---@param relativePath string
---@param context      string
---@return any
local function canonicaliseGeneralScalar(value, typeName, relativePath, context)
    if typeName == "any" then
        return value
    end
    if typeName == "string" or typeName == "file" then
        if type(value) ~= "string" then
            generalDataError(relativePath, context, "expected " .. typeName)
        end
        return value
    end
    if typeName == "bool" then
        if type(value) ~= "boolean" then
            generalDataError(relativePath, context, "expected bool")
        end
        return value
    end
    if typeName == "int" then
        if type(value) ~= "number" or math.type(value) ~= "integer" then
            generalDataError(relativePath, context, "expected int")
        end
        return value
    end
    if typeName == "float" then
        if type(value) ~= "number" then
            generalDataError(relativePath, context, "expected float")
        end
        return value + 0.0
    end
    if not generalDataScalarTypes[typeName] then
        generalDataError(relativePath, context, "unsupported type " .. tostring(typeName))
    end
    if not isArray(value) then
        generalDataError(relativePath, context, "expected JSON array for " .. typeName)
    end
    local result = Engine.resolveTypedDataValue(value, typeName)
    local sfTypeName = typeName:sub(4)
    local sfType = sf[sfTypeName]
    if sfType == nil or not Class.isInstance(result, sfType) then
        generalDataError(relativePath, context, "could not construct " .. typeName)
    end
    return result
end

---@param value        any
---@param typeName     string
---@param param        table
---@param relativePath string
---@param context      string
---@return any
local function canonicaliseGeneralValue(value, typeName, param, relativePath, context)
    if typeName == "list" then
        local itemType = param.itemType
        if type(itemType) ~= "string" or not bool(itemType) then
            generalDataError(relativePath, context, "list requires itemType")
        end
        if not generalDataScalarTypes[itemType] then
            generalDataError(relativePath, context, "unsupported itemType " .. tostring(itemType))
        end
        if not isArray(value) then
            generalDataError(relativePath, context, "expected JSON array")
        end
        local result = {}
        for index, item in ipairs(value) do
            result[index] = canonicaliseGeneralScalar(
                item, itemType, relativePath, context .. "[" .. tostring(index) .. "]"
            )
        end
        return result
    end
    if typeName == "dict" then
        local valueType = param.valueType
        if type(valueType) ~= "string" or not bool(valueType) then
            generalDataError(relativePath, context, "dict requires valueType")
        end
        if not generalDataScalarTypes[valueType] then
            generalDataError(relativePath, context, "unsupported valueType " .. tostring(valueType))
        end
        if not isDictionary(value) then
            generalDataError(relativePath, context, "expected JSON object with string keys")
        end
        local result = {}
        for key, item in pairs(value) do
            result[key] = canonicaliseGeneralScalar(
                item, valueType, relativePath, context .. "." .. key
            )
        end
        return result
    end
    return canonicaliseGeneralScalar(value, typeName, relativePath, context)
end

---@param payload      table
---@param relativePath string
local function canonicaliseGeneralData(payload, relativePath)
    local params = payload.params
    if not isDictionary(params) then
        generalDataError(relativePath, "schema", "params must be a JSON object")
    end
    local members = payload.members
    if not isDictionary(members) then
        generalDataError(relativePath, "schema", "members must be a JSON object")
    end
    for fieldName, param in pairs(params) do
        if type(param) ~= "table" then
            generalDataError(relativePath, "parameter " .. fieldName, "definition must be a JSON object")
        end
        local typeName = param.type
        if type(typeName) ~= "string" or not generalDataTypes[typeName] then
            generalDataError(
                relativePath, "parameter " .. fieldName, "unsupported type " .. tostring(typeName)
            )
        end
        if rawget(param, "defaultValue") == nil then
            generalDataError(relativePath, "parameter " .. fieldName, "defaultValue is required")
        end
        canonicaliseGeneralValue(
            param.defaultValue, typeName, param, relativePath, "parameter " .. fieldName .. ".defaultValue"
        )
    end
    for memberName, member in pairs(members) do
        if not isDictionary(member) then
            generalDataError(relativePath, "member " .. memberName, "must be a JSON object")
        end
        for fieldName in pairs(member) do
            if fieldName:sub(1, 1) ~= "_" and params[fieldName] == nil then
                generalDataError(relativePath, "member " .. memberName, "unknown field " .. fieldName)
            end
        end
        for fieldName, param in pairs(params) do
            local value = rawget(member, fieldName)
            if value == nil then
                generalDataError(relativePath, "member " .. memberName, "missing field " .. fieldName)
            end
            member[fieldName] = canonicaliseGeneralValue(
                value,
                param.type,
                param,
                relativePath,
                "member " .. memberName .. "." .. fieldName
            )
        end
    end
end

function DataLoading:init(data)
    self._data = data
end

---@param fileName string
---@return string, string
function DataLoading:splitCompound(fileName)
    local _ = self

    local name, extension = fileName:match("^(.-)(%..+)$")
    if name == nil then
        return fileName, ""
    end
    return name, extension or ""
end

---@param value Source.Data.JsonValue | userdata
---@param seen  table | nil
---@return Source.Data.JsonValue
function DataLoading:normaliseJsonNull(value, seen)
    if value == cjson.null then
        return nil
    end
    if type(value) ~= "table" then
        return value
    end
    seen = seen or {}
    if seen[value] ~= nil then
        return seen[value]
    end
    local result = {}
    seen[value] = result
    local arrayValue = true
    for key in pairs(value) do
        if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
            arrayValue = false
            break
        end
    end
    if arrayValue then
        for index = 1, #value do
            local item = value[index]
            result[index] = item == cjson.null and cjson.null or self:normaliseJsonNull(item, seen)
        end
        return result
    end
    for key, item in pairs(value) do
        local normalised = self:normaliseJsonNull(item, seen)
        if normalised ~= nil then
            result[self:normaliseJsonNull(key, seen)] = normalised
        end
    end
    return result
end

---@param relativePath string
---@return string
local function animationNameFromRelativePath(relativePath)
    local suffix = ".anim.json"
    assert(relativePath:sub(-#suffix) == suffix, "Invalid compressed animation file name: " .. relativePath)
    local name = relativePath:sub(1, -#suffix - 1)
    assert(bool(name), "Compressed animation name must not be empty")
    return name
end

---@param errorData FileBatchError | nil
---@return string
local function formatBatchError(errorData)
    if errorData == nil then
        return "File batch failed"
    end
    return string.format(
        "%s failed for %s: %s", errorData.operation, errorData.path, errorData.message
    )
end

---@param specs  FileBatchSpec[]
---@param onItem fun(item: FileBatchItem, snapshot: FileBatchSnapshot)
---@return integer
function DataLoading:drainFileBatch(specs, onItem)
    local _ = self

    assert(bool(asyncio), "asyncio is not initialised")
    local async = asyncio
    local job = async.start_file_batch(specs)
    while true do
        local snapshot = async.poll_file_batch(job, 64)
        if snapshot.state == "failed" then
            error(formatBatchError(snapshot.error))
        end
        if snapshot.state == "cancelled" then
            error("File batch was cancelled")
        end
        for _, item in ipairs(snapshot.items or {}) do
            onItem(item, snapshot)
        end
        if snapshot.state == "completed" and snapshot.drained then
            return snapshot.total
        end
    end
end

---@param spec         FileBatchSpec
---@param onFileLoaded fun() | nil
function DataLoading:_loadOneCategory(spec, onFileLoaded)
    local stage = self:beginInitialLoad()
    self:drainFileBatch({ spec }, function (item)
        self:applyInitialLoadItem(stage, item)
        if onFileLoaded ~= nil then
            onFileLoaded()
        end
    end)
    local field = assert(categoryFields[spec.category])
    if spec.category == "animations" then
        self._data._animationData = stage._animationData
    elseif spec.category == "curves" then
        self._data._curveData = stage._curveData
        self._data._curveTypes = stage._curveTypes
    else
        self._data[field] = stage[field]
    end
end

function DataLoading:beginInitialLoad()
    local _ = self

    return {
        _animationData = {},
        _curveData = {},
        _curveTypes = {},
        _textConfigData = {},
        _commonFunctionsData = {},
        _tilesetData = {},
        _autoTileData = {},
        _generalData = {},
        _aborted = false,
        _committed = false
    }
end

function DataLoading:applyInitialLoadItem(stage, item)
    assert(not stage._aborted, "Initial data stage is unavailable")
    assert(not stage._committed, "Initial data stage is already committed")
    local relativePath = Path.NormaliseSeparators(item.relativePath)
    local category = item.category
    Logging.debug("Loading %s: %s", category, relativePath)
    local payload = self:normaliseJsonNull(cjson.decode(item.content))
    ---@cast payload table<string, Source.Data.JsonValue>
    local name
    if category == "animations" then
        assert(payload.type == "compressedAnimation", "Invalid compressed animation type: " .. relativePath)
        assert(
            payload.frameEncoding == "base64+zlib", "Unsupported compressed animation frame encoding: " .. relativePath
        )
        name = animationNameFromRelativePath(relativePath)
        payload.type = nil
        stage._animationData[name] = Engine.AnimationData.new(payload)
    elseif category == "commonFunctions" then
        payload.type = nil
        name = self:splitCompound(relativePath)
        stage._commonFunctionsData[name] = payload
    elseif category == "tilesets" then
        payload.type = nil
        name = self:splitCompound(relativePath)
        stage._tilesetData[name] = Engine.Tileset.fromData(payload)
    elseif category == "autoTiles" then
        payload.type = nil
        name = self:splitCompound(relativePath)
        stage._autoTileData[name] = Engine.AutoTile.fromData(payload)
    elseif category == "general" then
        payload.type = nil
        name = self:splitCompound(relativePath)
        canonicaliseGeneralData(payload, relativePath)
        stage._generalData[name] = payload
    elseif category == "curves" then
        local curveType = payload.type
        payload.type = nil
        name = self:splitCompound(relativePath)
        if curveType == "curve" then
            local curveData = payload
            ---@cast curveData Engine.CurveData
            stage._curveData[name] = Curve.fromData(curveData)
        elseif curveType == "vector2Curve" then
            local curveData = payload
            ---@cast curveData Engine.Vector2CurveData
            stage._curveData[name] = Vector2Curve.fromData(curveData)
        elseif curveType == "vector3Curve" then
            local curveData = payload
            ---@cast curveData Engine.Vector3CurveData
            stage._curveData[name] = Vector3Curve.fromData(curveData)
        elseif curveType == "vector4Curve" then
            local curveData = payload
            ---@cast curveData Engine.Vector4CurveData
            stage._curveData[name] = Vector4Curve.fromData(curveData)
        else
            error("Invalid curve type " .. tostring(curveType) .. ": " .. relativePath)
        end
        stage._curveTypes[name] = curveType
    elseif category == "textConfigs" then
        assert(
            payload.type == "plainTextConfig" or payload.type == "richTextConfig",
            "Invalid text config type: " .. relativePath
        )
        name = self:splitCompound(relativePath)
        stage._textConfigData[name] = payload
    else
        error("Unknown initial data category: " .. tostring(category))
    end
    return name
end

function DataLoading:commitInitialLoad(stage, blueprints)
    assert(not stage._aborted, "Initial data stage is unavailable")
    assert(not stage._committed, "Initial data stage is already committed")
    self._data._animationData = stage._animationData
    self._data._curveData = stage._curveData
    self._data._curveTypes = stage._curveTypes
    self._data._textConfigData = stage._textConfigData
    self._data._plainTextConfigs = {}
    self._data._richTextConfigs = {}
    self._data._commonFunctionsData = stage._commonFunctionsData
    self._data._tilesetData = stage._tilesetData
    self._data._autoTileData = stage._autoTileData
    self._data._generalData = stage._generalData
    blueprints:clearGraphTemplates()
    stage._committed = true
end

function DataLoading:abortInitialLoad(stage)
    local _ = self

    if not stage._committed then
        stage._aborted = true
    end
end

function DataLoading:countLoadableFiles(dataRoot, needExt, recursive)
    if needExt ~= nil and tostring(needExt):sub(-4) ~= "json" then
        return 0
    end
    local suffix = needExt or ".json"
    local total = 0
    self:drainFileBatch({
        {
            category = "count",
            root = dataRoot,
            suffix = suffix,
            recursive = recursive == true,
            required = false
        }
    }, function ()
        total = total + 1
    end)
    return total
end

function DataLoading:loadAnimations(onFileLoaded)
    self:_loadOneCategory({
        category = "animations",
        root = "./Data/Animations",
        suffix = ".anim.json",
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadCommonFunctions(onFileLoaded)
    self:_loadOneCategory({
        category = "commonFunctions",
        root = "./Data/CommonFunctions",
        suffix = ".json",
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadTilesets(onFileLoaded)
    self:_loadOneCategory({
        category = "tilesets",
        root = "./Data/Tilesets",
        suffix = ".json",
        recursive = false,
        required = true
    }, onFileLoaded)
end

function DataLoading:loadAutoTiles(onFileLoaded)
    self:_loadOneCategory({
        category = "autoTiles",
        root = "./Data/AutoTiles",
        suffix = ".json",
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadGeneralData(onFileLoaded)
    self:_loadOneCategory({
        category = "general",
        root = "./Data/General",
        suffix = ".json",
        recursive = false,
        required = true
    }, onFileLoaded)
end

function DataLoading:loadCurves(onFileLoaded)
    self:_loadOneCategory({
        category = "curves",
        root = "./Data/Curves",
        suffix = ".json",
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadTextConfigs(onFileLoaded)
    self:_loadOneCategory({
        category = "textConfigs",
        root = "./Data/TextConfigs",
        suffix = ".json",
        recursive = true,
        required = true
    }, onFileLoaded)
end

return class(DataLoading)
