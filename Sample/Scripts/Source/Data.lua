local cjson = require("cjson")
local Engine = require("Engine")
local NodeCompiler = require("Global.Utils.NodeCompiler")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
---@type Global.Utils.Path.Module
local Path = require("Global.Utils.Path")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local Curve = Engine.Curve
local Vector2Curve = Engine.Vector2Curve
local Vector3Curve = Engine.Vector3Curve
local Vector4Curve = Engine.Vector4Curve
local ComponentsFunctions = GlobalFunctions.Components
local ManagerFunctions = GlobalFunctions.Manager
local GeneralDataKey = GeneralEnum.GeneralDataKey
local PlainTextConfig = Engine.PlainTextConfig
local RichTextConfig = Engine.RichTextConfig
local TextGradientConfig = Engine.TextGradientConfig
local TextGlowConfig = Engine.TextGlowConfig
local TextOutlineConfig = Engine.TextOutlineConfig
local TextStyle = Engine.TextStyle

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

local nilGraphParentClass = {}
local graphTemplates = {}
local nodeCompilerContext = {
    moduleCandidates = function (prefix)
        return { "Source." .. prefix, "Global." .. prefix }
    end,
    roots = {
        {
            name = "Engine",
            value = Engine
        },
        {
            name = "GlobalCore",
            value = GlobalCore
        }
    }
}

local function clearGraphTemplates()
    graphTemplates = {}
end

local categoryFields = {
    animations = "_animationData",
    commonFunctions = "_commonFunctionsData",
    tilesets = "_tilesetData",
    autoTiles = "_autoTileData",
    general = "_generalData",
    curves = "_curveData",
    textConfigs = "_textConfigData"
}

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

---@param fileName string
---@return string, string
local function splitCompound(fileName)
    local name, extension = fileName:match("^(.-)(%..+)$")
    if name == nil then
        return fileName, ""
    end
    return name, extension or ""
end

---@param value Source.Data.JsonValue | userdata
---@param seen  table | nil
---@return Source.Data.JsonValue
local function normaliseJsonNull(value, seen)
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
    local isArray = true
    for key in pairs(value) do
        if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
            isArray = false
            break
        end
    end
    if isArray then
        for index = 1, #value do
            local item = value[index]
            result[index] = item == cjson.null and cjson.null or normaliseJsonNull(item, seen)
        end
        return result
    end
    for key, item in pairs(value) do
        local normalised = normaliseJsonNull(item, seen)
        if normalised ~= nil then
            result[normaliseJsonNull(key, seen)] = normalised
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
local function drainFileBatch(specs, onItem)
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
local function loadOneCategory(spec, onFileLoaded)
    local stage = Data.beginInitialLoad()
    drainFileBatch({ spec }, function (item)
        Data.applyInitialLoadItem(stage, item)
        if onFileLoaded ~= nil then
            onFileLoaded()
        end
    end)
    local field = assert(categoryFields[spec.category])
    if spec.category == "animations" then
        Data._animationData = stage._animationData
    elseif spec.category == "curves" then
        Data._curveData = stage._curveData
        Data._curveTypes = stage._curveTypes
    else
        Data[field] = stage[field]
    end
end

function Data.beginInitialLoad()
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

function Data.applyInitialLoadItem(stage, item)
    assert(not stage._aborted, "Initial data stage is unavailable")
    assert(not stage._committed, "Initial data stage is already committed")
    local relativePath = Path.NormaliseSeparators(item.relativePath)
    local category = item.category
    Logging.debug("Loading %s: %s", category, relativePath)
    local payload = normaliseJsonNull(cjson.decode(item.content))
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
        name = splitCompound(relativePath)
        stage._commonFunctionsData[name] = payload
    elseif category == "tilesets" then
        payload.type = nil
        name = splitCompound(relativePath)
        stage._tilesetData[name] = Engine.Tileset.fromData(payload)
    elseif category == "autoTiles" then
        payload.type = nil
        name = splitCompound(relativePath)
        stage._autoTileData[name] = Engine.AutoTile.fromData(payload)
    elseif category == "general" then
        payload.type = nil
        name = splitCompound(relativePath)
        stage._generalData[name] = payload
    elseif category == "curves" then
        local curveType = payload.type
        payload.type = nil
        name = splitCompound(relativePath)
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
        name = splitCompound(relativePath)
        stage._textConfigData[name] = payload
    else
        error("Unknown initial data category: " .. tostring(category))
    end
    return name
end

function Data.commitInitialLoad(stage)
    assert(not stage._aborted, "Initial data stage is unavailable")
    assert(not stage._committed, "Initial data stage is already committed")
    Data._animationData = stage._animationData
    Data._curveData = stage._curveData
    Data._curveTypes = stage._curveTypes
    Data._textConfigData = stage._textConfigData
    Data._plainTextConfigs = {}
    Data._richTextConfigs = {}
    Data._commonFunctionsData = stage._commonFunctionsData
    Data._tilesetData = stage._tilesetData
    Data._autoTileData = stage._autoTileData
    Data._generalData = stage._generalData
    clearGraphTemplates()
    stage._committed = true
end

function Data.abortInitialLoad(stage)
    if not stage._committed then
        stage._aborted = true
    end
end

function Data.getDataKinds()
    return Data.dataKinds
end

function Data.countLoadableFiles(dataRoot, needExt, _defaultType, recursive)
    if needExt ~= nil and tostring(needExt):sub(-4) ~= "json" then
        return 0
    end
    local suffix = needExt or ".json"
    local total = 0
    drainFileBatch({
        {
            category = "count",
            root = dataRoot,
            suffix = suffix,
            recursive = recursive == true,
            required = false
        }
    },
        function (_, snapshot)
            total = snapshot.total
        end)
    return total
end

function Data.loadAnimations(onFileLoaded)
    loadOneCategory({
        category = "animations",
        root = Engine.getAnimationCacheRoot(),
        suffix = ".anim.json",
        recursive = true,
        required = true
    }, onFileLoaded)
end

function Data.loadCommonFunctions(onFileLoaded)
    loadOneCategory({
        category = "commonFunctions",
        root = "./Data/CommonFunctions",
        suffix = ".json",
        recursive = false,
        required = true
    }, onFileLoaded)
    clearGraphTemplates()
end

function Data.loadTilesets(onFileLoaded)
    loadOneCategory({
        category = "tilesets",
        root = "./Data/Tilesets",
        suffix = ".json",
        recursive = false,
        required = true
    }, onFileLoaded)
end

function Data.loadAutoTiles(onFileLoaded)
    loadOneCategory({
        category = "autoTiles",
        root = "./Data/AutoTiles",
        suffix = ".json",
        recursive = false,
        required = false
    }, onFileLoaded)
end

function Data.loadGeneralData(onFileLoaded)
    loadOneCategory({
        category = "general",
        root = "./Data/General",
        suffix = ".json",
        recursive = false,
        required = true
    }, onFileLoaded)
    clearGraphTemplates()
end

function Data.loadCurves(onFileLoaded)
    loadOneCategory({
        category = "curves",
        root = "./Data/Curves",
        suffix = ".json",
        recursive = true,
        required = false
    }, onFileLoaded)
    Data._plainTextConfigs = {}
    Data._richTextConfigs = {}
end

function Data.loadTextConfigs(onFileLoaded)
    loadOneCategory({
        category = "textConfigs",
        root = "./Data/TextConfigs",
        suffix = ".json",
        recursive = true,
        required = true
    }, onFileLoaded)
    Data._plainTextConfigs = {}
    Data._richTextConfigs = {}
end

function Data.splitCompound(fileName)
    return splitCompound(fileName)
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

local plainTextConfigFields = {
    type = true,
    name = true,
    font = true,
    characterSize = true,
    style = true,
    slantAngle = true,
    fillColor = true,
    letterSpacing = true,
    lineSpacing = true,
    lineAlignment = true,
    outline = true,
    glow = true,
    gradient = true
}

local richTextConfigFields = {
    type = true,
    name = true,
    font = true,
    lineAlignment = true,
    defaultStyle = true,
    styleOrder = true,
    styles = true,
    glow = true,
    gradient = true
}

local textStyleFields = { bold = true, italic = true, underlined = true, strikeThrough = true }

local textStyleFieldOrder = { "bold", "italic", "underlined", "strikeThrough" }

local richTextStyleFields = {
    characterSize = true,
    style = true,
    fillColor = true,
    letterSpacing = true,
    lineSpacing = true,
    outline = true
}

local textOutlineFields = { color = true, thickness = true }

local textGlowFields = { enabled = true, color = true, radius = true, intensity = true }

local textGradientFields = { enabled = true, direction = true, curve = true }

---@param sourceName string
---@param message    string
local function textConfigError(sourceName, message)
    error(message .. " in text config " .. sourceName, 3)
end

---@param value      table
---@param fields     table<string, boolean>
---@param sourceName string
local function textConfigOnlyFields(value, fields, sourceName)
    for key in pairs(value) do
        if type(key) ~= "string" or fields[key] ~= true then
            textConfigError(sourceName, "Unknown field " .. tostring(key))
        end
    end
end

---@param value      boolean
---@param sourceName string
---@return boolean
local function textConfigBoolean(value, sourceName)
    if type(value) ~= "boolean" then
        textConfigError(sourceName, "Expected a boolean")
    end
    return value
end

---@param value      string
---@param sourceName string
---@param allowEmpty boolean
---@return string
local function textConfigString(value, sourceName, allowEmpty)
    if type(value) ~= "string" then
        textConfigError(sourceName, "Expected a string")
    end
    if not allowEmpty and not bool(value) then
        textConfigError(sourceName, "Expected a non-empty string")
    end
    return value
end

---@param value      any
---@param sourceName string
---@param minimum    number | nil
---@param maximum    number | nil
---@return number
local function textConfigNumber(value, sourceName, minimum, maximum)
    if type(value) ~= "number" or value ~= value or value <= -math.huge or value >= math.huge then
        textConfigError(sourceName, "Expected a finite number")
    end
    if minimum ~= nil and value < minimum then
        textConfigError(sourceName, "Expected a number greater than or equal to " .. tostring(minimum))
    end
    if maximum ~= nil and value > maximum then
        textConfigError(sourceName, "Expected a number less than or equal to " .. tostring(maximum))
    end
    return value
end

---@param value      any
---@param sourceName string
---@param minimum    integer | nil
---@param maximum    integer | nil
---@return integer
local function textConfigInteger(value, sourceName, minimum, maximum)
    local number = textConfigNumber(value, sourceName, minimum, maximum)
    if number % 1 ~= 0 then
        textConfigError(sourceName, "Expected an integer")
    end
    ---@cast number integer
    return number
end

---@param value      table
---@param sourceName string
---@return integer
local function textConfigArrayLength(value, sourceName)
    local length = 0
    for key in pairs(value) do
        if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
            textConfigError(sourceName, "Expected an array")
        end
        length = math.max(length, key)
    end
    for index = 1, length do
        if value[index] == nil then
            textConfigError(sourceName, "Array entries must be contiguous")
        end
    end
    return length
end

---@param value      integer[]
---@param sourceName string
---@return sf.Color
local function textColourFromData(value, sourceName)
    local length = textConfigArrayLength(value, sourceName)
    if length ~= 3 and length ~= 4 then
        textConfigError(sourceName, "Expected three or four colour channels")
    end
    local alpha = 255
    if length == 4 then
        alpha = textConfigInteger(value[4], sourceName .. "[4]", 0, 255)
    end
    return sf.Color.new(
        textConfigInteger(value[1], sourceName .. "[1]", 0, 255),
        textConfigInteger(value[2], sourceName .. "[2]", 0, 255),
        textConfigInteger(value[3], sourceName .. "[3]", 0, 255), alpha
    )
end

---@param value      table<string, boolean>
---@param sourceName string
---@param requireAll boolean
---@return table<string, boolean>
local function textStyleFlagsFromData(value, sourceName, requireAll)
    textConfigOnlyFields(value, textStyleFields, sourceName)
    local flags = {}
    for _, field in ipairs(textStyleFieldOrder) do
        if value[field] ~= nil then
            flags[field] = textConfigBoolean(value[field], sourceName .. "." .. field)
        elseif requireAll then
            textConfigError(sourceName, "Missing " .. field)
        end
    end
    return flags
end

---@param value      table<string, boolean>
---@param sourceName string
---@return integer
local function textStyleFromData(value, sourceName)
    local flags = textStyleFlagsFromData(value, sourceName, true)
    local style = sf.Text.Style.Regular
    if flags.bold then
        style = style + sf.Text.Style.Bold
    end
    if flags.italic then
        style = style + sf.Text.Style.Italic
    end
    if flags.underlined then
        style = style + sf.Text.Style.Underlined
    end
    if flags.strikeThrough then
        style = style + sf.Text.Style.StrikeThrough
    end
    return style
end

---@param value      string
---@param sourceName string
---@return sf.Text.LineAlignment
local function textAlignmentFromData(value, sourceName)
    local alignments = {
        default = sf.Text.LineAlignment.Default,
        left = sf.Text.LineAlignment.Left,
        center = sf.Text.LineAlignment.Center,
        right = sf.Text.LineAlignment.Right
    }
    local name = textConfigString(value, sourceName, false)
    if alignments[name] == nil then
        textConfigError(sourceName, "Invalid line alignment " .. name)
    end
    local alignment = alignments[name]
    ---@cast alignment sf.Text.LineAlignment
    return alignment
end

---@param value      string
---@param sourceName string
---@return sf.Font
local function textFontFromData(value, sourceName)
    local path = textConfigString(value, sourceName, true)
    if not bool(path) then
        ---@type sf.Font|nil
        local engineDefaultFont = Engine.DefaultFont
        local defaultFont = assert(engineDefaultFont, "Default font is unavailable for text config " .. sourceName)
        return defaultFont
    end
    local font = assert(ManagerFunctions.loadFont(path), "Font not found for text config " .. sourceName .. ": " .. path)
    return font
end

---@param value      { color: integer[], thickness: number }
---@param sourceName string
---@return Engine.TextOutlineConfig
local function textOutlineFromData(value, sourceName)
    textConfigOnlyFields(value, textOutlineFields, sourceName)
    return TextOutlineConfig.new({
        color = textColourFromData(value.color, sourceName .. ".color"),
        thickness = textConfigNumber(value.thickness, sourceName .. ".thickness", 0.0, 32.0)
    })
end

---@param value      { enabled: boolean, color: integer[], radius: number, intensity: number }
---@param sourceName string
---@return Engine.TextGlowConfig
local function textGlowFromData(value, sourceName)
    textConfigOnlyFields(value, textGlowFields, sourceName)
    return TextGlowConfig.new({
        enabled = textConfigBoolean(value.enabled, sourceName .. ".enabled"),
        color = textColourFromData(value.color, sourceName .. ".color"),
        radius = textConfigNumber(value.radius, sourceName .. ".radius", 0.0, 64.0),
        intensity = textConfigNumber(value.intensity, sourceName .. ".intensity", 0.0, 1.0)
    })
end

---@param value      { enabled: boolean, direction: string, curve: string }
---@param sourceName string
---@return Engine.TextGradientConfig
local function textGradientFromData(value, sourceName)
    textConfigOnlyFields(value, textGradientFields, sourceName)
    local enabled = textConfigBoolean(value.enabled, sourceName .. ".enabled")
    local direction = textConfigString(value.direction, sourceName .. ".direction", false)
    if direction ~= "horizontal" and direction ~= "vertical" then
        textConfigError(sourceName .. ".direction", "Invalid gradient direction " .. direction)
    end
    local curveName = textConfigString(value.curve, sourceName .. ".curve", true)
    local curve = nil
    if bool(curveName) then
        curve = Data.getVector4Curve(curveName)
    elseif enabled then
        textConfigError(sourceName .. ".curve", "Enabled gradient requires a curve")
    end
    return TextGradientConfig.new({
        enabled = enabled,
        direction = direction,
        curve = curve
    })
end

---@param value      table
---@param sourceName string
---@param requireAll boolean
---@return Engine.TextStyle
local function richTextStyleFromData(value, sourceName, requireAll)
    textConfigOnlyFields(value, richTextStyleFields, sourceName)
    ---@type table<string, integer|number|boolean|sf.Color>
    local style = {}
    if value.characterSize ~= nil then
        style.characterSize = textConfigInteger(value.characterSize, sourceName .. ".characterSize", 1, 512)
    elseif requireAll then
        textConfigError(sourceName, "Missing characterSize")
    end
    if value.style ~= nil then
        local flags = textStyleFlagsFromData(value.style, sourceName .. ".style", requireAll)
        for field, enabled in pairs(flags) do
            style[field] = enabled
        end
    elseif requireAll then
        textConfigError(sourceName, "Missing style")
    end
    if value.fillColor ~= nil then
        style.fillColor = textColourFromData(value.fillColor, sourceName .. ".fillColor")
    elseif requireAll then
        textConfigError(sourceName, "Missing fillColor")
    end
    if value.letterSpacing ~= nil then
        style.letterSpacing = textConfigNumber(value.letterSpacing, sourceName .. ".letterSpacing", 0.1, 10.0)
    elseif requireAll then
        textConfigError(sourceName, "Missing letterSpacing")
    end
    if value.lineSpacing ~= nil then
        style.lineSpacing = textConfigNumber(value.lineSpacing, sourceName .. ".lineSpacing", 0.1, 10.0)
    elseif requireAll then
        textConfigError(sourceName, "Missing lineSpacing")
    end
    if value.outline ~= nil then
        local outlineSource = sourceName .. ".outline"
        textConfigOnlyFields(value.outline, textOutlineFields, outlineSource)
        if value.outline.color ~= nil then
            style.outlineColor = textColourFromData(value.outline.color, outlineSource .. ".color")
        elseif requireAll then
            textConfigError(outlineSource, "Missing color")
        end
        if value.outline.thickness ~= nil then
            style.outlineThickness = textConfigNumber(value.outline.thickness, outlineSource .. ".thickness", 0.0, 32.0)
        elseif requireAll then
            textConfigError(outlineSource, "Missing thickness")
        end
    elseif requireAll then
        textConfigError(sourceName, "Missing outline")
    end
    return TextStyle.new(style)
end

---@param name  string
---@param value table
---@return Engine.PlainTextConfig
local function buildPlainTextConfig(name, value)
    local sourceName = tostring(name)
    textConfigOnlyFields(value, plainTextConfigFields, sourceName)
    local configType = textConfigString(value.type, sourceName .. ".type", false)
    if configType ~= "plainTextConfig" then
        textConfigError(sourceName .. ".type", "Expected plainTextConfig")
    end
    return PlainTextConfig.new({
        type = configType,
        name = textConfigString(value.name, sourceName .. ".name", true),
        font = textFontFromData(value.font, sourceName .. ".font"),
        characterSize = textConfigInteger(value.characterSize, sourceName .. ".characterSize", 1, 512),
        style = textStyleFromData(value.style, sourceName .. ".style"),
        slantAngle = value.slantAngle == nil and 0.0
            or textConfigNumber(value.slantAngle, sourceName .. ".slantAngle", -45.0, 45.0),
        fillColor = textColourFromData(value.fillColor, sourceName .. ".fillColor"),
        letterSpacing = textConfigNumber(value.letterSpacing, sourceName .. ".letterSpacing", 0.1, 10.0),
        lineSpacing = textConfigNumber(value.lineSpacing, sourceName .. ".lineSpacing", 0.1, 10.0),
        lineAlignment = textAlignmentFromData(value.lineAlignment, sourceName .. ".lineAlignment"),
        outline = textOutlineFromData(value.outline, sourceName .. ".outline"),
        glow = textGlowFromData(value.glow, sourceName .. ".glow"),
        gradient = textGradientFromData(value.gradient, sourceName .. ".gradient")
    })
end

---@param name  string
---@param value table
---@return Engine.RichTextConfig
local function buildRichTextConfig(name, value)
    local sourceName = tostring(name)
    textConfigOnlyFields(value, richTextConfigFields, sourceName)
    local configType = textConfigString(value.type, sourceName .. ".type", false)
    if configType ~= "richTextConfig" then
        textConfigError(sourceName .. ".type", "Expected richTextConfig")
    end
    local styleOrderLength = textConfigArrayLength(value.styleOrder, sourceName .. ".styleOrder")
    local styleOrder = {}
    local styles = {}
    local orderedStyles = {}
    for index = 1, styleOrderLength do
        local styleName = textConfigString(
            value.styleOrder[index], sourceName .. ".styleOrder[" .. tostring(index) .. "]", false
        )
        if styleName == "default" or styleName:find("#", 1, true) ~= nil then
            textConfigError(sourceName .. ".styleOrder", "Reserved rich text style name " .. styleName)
        end
        if orderedStyles[styleName] then
            textConfigError(sourceName .. ".styleOrder", "Duplicate rich text style " .. styleName)
        end
        if value.styles[styleName] == nil then
            textConfigError(sourceName .. ".styles", "Missing rich text style " .. styleName)
        end
        orderedStyles[styleName] = true
        styleOrder[index] = styleName
        styles[styleName] = richTextStyleFromData(value.styles[styleName], sourceName .. ".styles." .. styleName, false)
    end
    for styleName in pairs(value.styles) do
        if type(styleName) ~= "string" then
            textConfigError(sourceName .. ".styles", "Rich text style names must be strings")
        end
        if not orderedStyles[styleName] then
            textConfigError(sourceName .. ".styles", "Rich text style " .. styleName .. " is not listed in styleOrder")
        end
    end
    return RichTextConfig.new({
        type = configType,
        name = textConfigString(value.name, sourceName .. ".name", true),
        font = textFontFromData(value.font, sourceName .. ".font"),
        lineAlignment = textAlignmentFromData(value.lineAlignment, sourceName .. ".lineAlignment"),
        defaultStyle = richTextStyleFromData(value.defaultStyle, sourceName .. ".defaultStyle", true),
        styleOrder = styleOrder,
        styles = styles,
        glow = textGlowFromData(value.glow, sourceName .. ".glow"),
        gradient = textGradientFromData(value.gradient, sourceName .. ".gradient")
    })
end

function Data.getPlainTextConfig(name)
    local cached = Data._plainTextConfigs[name]
    if cached ~= nil then
        return cached
    end
    local value = requireNamedValue(Data._textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "plainTextConfig", "Text config is not plain text: " .. tostring(name))
    cached = buildPlainTextConfig(name, value)
    Data._plainTextConfigs[name] = cached
    return cached
end

function Data.getRichTextConfig(name)
    local cached = Data._richTextConfigs[name]
    if cached ~= nil then
        return cached
    end
    local value = requireNamedValue(Data._textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "richTextConfig", "Text config is not rich text: " .. tostring(name))
    cached = buildRichTextConfig(name, value)
    Data._richTextConfigs[name] = cached
    return cached
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

---@return string[]
local function loadBlueprintClassPaths()
    if Data._blueprintClassPaths ~= nil then
        return Data._blueprintClassPaths
    end
    local paths = {}
    drainFileBatch({
        {
            category = "blueprints",
            root = "./Data/Blueprints",
            suffix = ".json",
            recursive = true,
            required = false
        }
    },
        function (item)
            local relative = Path.NormaliseSeparators(tostring(item.relativePath))
            relative = relative:gsub("%.json$", "")
            local classPath = "Data.Blueprints." .. relative:gsub("/", ".")
            paths[#paths + 1] = classPath
            Data._blueprintClassData[classPath] = item.content
        end)
    table.sort(paths)
    local index = {}
    local leafMatches = {}
    for _, classPath in ipairs(paths) do
        index[classPath:gsub("%.", "_")] = classPath
        local leaf = classPath:match("([^%.]+)$")
        local matches = leafMatches[leaf]
        if matches == nil then
            matches = {}
            leafMatches[leaf] = matches
        end
        matches[#matches + 1] = classPath
    end
    for leaf, matches in pairs(leafMatches) do
        if #matches == 1 then
            index[leaf] = matches[1]
        end
    end
    Data._blueprintClassPaths = paths
    Data._blueprintClassPathIndex = index
    return paths
end

function Data.resolveClassPath(className)
    if type(className) ~= "string" then
        return ""
    end
    className = className:match("^%s*(.-)%s*$")
    if not bool(className) then
        return ""
    end
    if Data._classDict:containsCached(className) then
        return className
    end
    local cachedPath = Data._classDict:findCachedPathByName(className)
    if cachedPath ~= nil then
        return cachedPath
    end
    loadBlueprintClassPaths()
    local blueprintPath = assert(Data._blueprintClassPathIndex)[className]
    if blueprintPath ~= nil then
        return blueprintPath
    end
    return className
end

function Data.getCommonFunction(name)
    local data = Data._commonFunctionsData[name]
    if data == nil then
        local path = "./Data/CommonFunctions/" .. tostring(name) .. ".json"
        assert(Engine.jsonExists(path), "Common function not found: " .. tostring(name))
        local loadedData = normaliseJsonNull(Engine.getJSONData(path))
        ---@cast loadedData Source.Data.GraphData
        data = loadedData
        Data._commonFunctionsData[name] = loadedData
    end
    return Data.genGraphFromData(data)
end

---@param data        Source.Data.GraphData
---@param parentClass Class.ClassType<any> | nil
---@return Engine.Graph
local function compileGraphTemplate(data, parentClass)
    local templates = graphTemplates[data]
    if templates == nil then
        templates = {}
        graphTemplates[data] = templates
    end
    local parentKey = parentClass or nilGraphParentClass
    local template = templates[parentKey]
    if template ~= nil then
        return template
    end
    local nodes = {}
    local links = {}
    local eventParams = deepcopy(data.eventParams or {})
    local startNodes = deepcopy(data.startNodes or {})
    for key, valueDict in pairs(data.nodeGraph) do
        nodes[key] = {}
        for _, node in ipairs(valueDict.nodes or {}) do
            local nodeData = deepcopy(node)
            nodeData.pos = nil
            local resolvedDefinition = NodeCompiler.compile(nodeData.nodeFunction, parentClass, nodeCompilerContext)
            assert(
                resolvedDefinition ~= nil,
                "Function " .. tostring(nodeData.nodeFunction) .. " not found while compiling graph"
            )
            nodes[key][#nodes[key] + 1] = Engine.DataNode.new(
                nodeData.nodeFunction, nodeData.params, resolvedDefinition
            )
        end
        links[key] = deepcopy(valueDict.links or {})
        if eventParams[key] == nil and (#nodes[key] > 0 or startNodes[key] ~= nil) then
            local eventDefinition = NodeCompiler.compile(key, parentClass, nodeCompilerContext)
                or NodeCompiler.compile("self." .. key, parentClass, nodeCompilerContext)
            if bool(eventDefinition) and bool(eventDefinition.paramNames) then
                eventParams[key] = deepcopy(eventDefinition.paramNames)
            end
        end
    end
    template = Engine.Graph.new(
        data.parent or "NOT_WRITTEN", parentClass, nil, nodes, links, nil, startNodes, eventParams
    )
    templates[parentKey] = template
    return template
end

function Data.genGraphFromData(data, parent, parentClass)
    return compileGraphTemplate(data, parentClass):instantiate(parent)
end

---@param actor Engine.Actor
local function applyActorGenerationClassVars(actor)
    actor:setTranslation(actor.defaultTranslation)
    actor:setRotation(tonumber(actor.defaultRotation) or 0.0)
    actor:setScale(actor.defaultScale)
    actor:setOrigin(actor.defaultOrigin)
end

---@param actor Engine.Actor
---@param key   string
---@param value Source.Data.ClassVarValue
---@return Source.Data.ClassVarValue
local function resolveClassVarChangeValue(actor, key, value)
    if type(value) == "string" and not bool(value) then
        local configName, settingName = Engine.resolveConfigVar(Class.type(actor), key)
        if configName ~= nil then
            local SourceSystem = require("Source.System")

            local resolved = SourceSystem.getConfigValue(configName, settingName)
            return type(resolved) == "string" and resolved or tostring(resolved)
        end
    end
    local fieldMetadata, declaringModule = Engine.resolveMemberMetadata(Class.type(actor), key)
    local targetType = fieldMetadata ~= nil and fieldMetadata.type
        or Engine.resolveAttrValueType(Class.type(actor), key)
    if type(value) == "string" and Engine.shouldEvalValueType(targetType) then
        return Engine.evalDataExpression(value)
    end
    if targetType ~= "any" then
        return deepcopy(Engine.resolveTypedDataValue(value, targetType, nil, declaringModule))
    end
    return deepcopy(value)
end

---@param actor Engine.Actor
---@param key   string
---@return boolean
local function isBlueprintOnlyClassVar(actor, key)
    local fieldMetadata = Engine.resolveMemberMetadata(Class.type(actor), key)
    local value = fieldMetadata ~= nil and fieldMetadata.Meta ~= nil and fieldMetadata.Meta.BlueprintOnly or nil
    return value == true
end

---@param actor     Engine.Actor
---@param applyRect boolean
local function applyActorTextureClassVars(actor, applyRect)
    local texturePath = actor.texturePath
    if texturePath == nil then
        texturePath = ""
    end
    local texture = bool(texturePath) and ManagerFunctions.loadCharacter(texturePath) or nil
    if texture ~= nil then
        actor:setTexture(texture, true)
    end
    local rect = actor.defaultRect
    if rect ~= nil and applyRect then
        actor:setTextureRect(rect)
    end
end

---@param actor Engine.Actor
local function normaliseActorClassVarObjects(actor)
    if actor.material ~= nil and not Class.isInstance(actor.material, Engine.Material) then
        actor.material = Engine.Material.fromData(actor.material)
    end
    actor:normaliseAutoSoundParams()
end

---@param actor   Engine.Actor
---@param changes table<string, Source.Data.ClassVarValue>
local function applyActorClassVarChanges(actor, changes)
    ---@type Source.Data.GeneratedActor
    local generatedActor = actor
    local storedChanges = generatedActor._classVarChanges
    if storedChanges == nil then
        storedChanges = {}
    else
        storedChanges = deepcopy(storedChanges)
    end
    for key, value in pairs(changes) do
        if type(key) == "string" and not isBlueprintOnlyClassVar(actor, key) then
            storedChanges[key] = deepcopy(value)
            actor[key] = resolveClassVarChangeValue(actor, key, value)
        end
    end
    if bool(storedChanges) then
        generatedActor._classVarChanges = storedChanges
    end
    ComponentsFunctions.normaliseInstanceComponents(actor)
    normaliseActorClassVarObjects(actor)
    if changes.shaderPath ~= nil then
        actor:setShaderPath(actor.shaderPath or "")
    end
    if changes.texturePath ~= nil or changes.defaultRect ~= nil then
        applyActorTextureClassVars(actor, true)
    end
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function Data.genActorFromClassPath(classPath, tag, classVarChanges)
    if not bool(classPath) then
        return nil
    end
    local classModel = Data.getClass(classPath)
    if classModel == nil then
        return nil
    end
    local texturePath = classModel.texturePath or ""
    local defaultRect = classModel.defaultRect
    local texture = bool(texturePath) and ManagerFunctions.loadCharacter(texturePath) or nil
    local actor = classModel.GenActor(classModel, texture, defaultRect, tag)
    actor:setMapTag(tag == nil and "" or tostring(tag))
    actor.texturePath = texturePath
    applyActorGenerationClassVars(actor)
    local graph = Data._classDict:instantiateGraph(classPath, actor)
    if graph ~= nil then
        actor:setGraph(graph)
    end
    if classVarChanges ~= nil then
        applyActorClassVarChanges(actor, classVarChanges)
        applyActorGenerationClassVars(actor)
    end
    return actor
end

function Data.genActorFromClassName(className, tag)
    return Data.genActorFromClassPath(Data.resolveClassPath(className), tag)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function Data.genActorFromData(actorData, layerName, classVarChanges)
    local tag = actorData.tag
    local position = actorData.position
    local blueprint = actorData.bp
    if blueprint == nil then
        Logging.warning("Actor %s in layer %s has no bp", tostring(tag), tostring(layerName))
        return nil
    end
    blueprint = Data.resolveClassPath(blueprint)
    local actor = Data.genActorFromClassPath(blueprint, tag, classVarChanges)
    if actor == nil then
        return nil
    end
    applyActorGenerationClassVars(actor)
    actor:setMapPosition(position)
    return actor
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

Class.registerService("blueprint.classGraphData", function (className)
    local classPath = Data.resolveClassPath(className)
    local classData = Data.getClassData(classPath)
    return classData ~= nil and classData.graph or nil
end)

Class.registerService("blueprint.classDataByPath", function (classPath)
    if Data._blueprintClassPaths == nil then
        loadBlueprintClassPaths()
    end
    local data = Data._blueprintClassData[classPath]
    if type(data) == "string" then
        local loadedData = normaliseJsonNull(cjson.decode(data))
        ---@cast loadedData table<string, Source.Data.JsonValue>
        data = loadedData
        Data._blueprintClassData[classPath] = loadedData
    end
    if data ~= nil then
        return data
    end
    local relative = classPath:match("^Data%.Blueprints%.(.+)$")
    if relative == nil then
        return nil
    end
    local path = "./Data/Blueprints/" .. relative:gsub("%.", "/") .. ".json"
    if not Engine.jsonExists(path) then
        return nil
    end
    local loadedData = normaliseJsonNull(Engine.getJSONData(path))
    ---@cast loadedData table<string, Source.Data.JsonValue>
    Data._blueprintClassData[classPath] = loadedData
    return loadedData
end)

Class.registerService("blueprint.invalidateClassData", function (classPath)
    local data = Data._blueprintClassData[classPath]
    if type(data) == "table" and data.graph ~= nil then
        graphTemplates[data.graph] = nil
    end
    Data._blueprintClassData[classPath] = nil
end)

Class.registerService("blueprint.compileGraph", function (graphData, parentClass)
    return compileGraphTemplate(graphData, parentClass)
end)

Class.registerService("blueprint.instantiateGraphTemplate", function (template, parent)
    return template:instantiate(parent)
end)

Class.registerService("blueprint.createGraph", function (graphData, parent, parentClass)
    return Data.genGraphFromData(graphData, parent, parentClass)
end)

return Data
