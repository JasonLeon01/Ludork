local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")

local ManagerFunctions = GlobalFunctions.Manager
local PlainTextConfig = Engine.PlainTextConfig
local RichTextConfig = Engine.RichTextConfig
local TextGradientConfig = Engine.TextGradientConfig
local TextGlowConfig = Engine.TextGlowConfig
local TextOutlineConfig = Engine.TextOutlineConfig
local TextStyle = Engine.TextStyle

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

local textStyleFields = {
    bold = true,
    italic = true,
    underlined = true,
    strikeThrough = true
}

local textStyleFieldOrder = { "bold", "italic", "underlined", "strikeThrough" }

local richTextStyleFields = {
    characterSize = true,
    style = true,
    fillColor = true,
    letterSpacing = true,
    lineSpacing = true,
    outline = true
}

local textOutlineFields = {
    color = true,
    thickness = true
}

local textGlowFields = {
    enabled = true,
    color = true,
    radius = true,
    intensity = true
}

local textGradientFields = {
    enabled = true,
    direction = true,
    curve = true
}

local DataTextConfigs = {}

function DataTextConfigs:init(data)
    self._data = data
end

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

---@param data       table
---@param value      { enabled: boolean, direction: string, curve: string }
---@param sourceName string
---@return Engine.TextGradientConfig
local function textGradientFromData(data, value, sourceName)
    textConfigOnlyFields(value, textGradientFields, sourceName)
    local enabled = textConfigBoolean(value.enabled, sourceName .. ".enabled")
    local direction = textConfigString(value.direction, sourceName .. ".direction", false)
    if direction ~= "horizontal" and direction ~= "vertical" then
        textConfigError(sourceName .. ".direction", "Invalid gradient direction " .. direction)
    end
    local curveName = textConfigString(value.curve, sourceName .. ".curve", true)
    local curve = nil
    if bool(curveName) then
        curve = data.getVector4Curve(curveName)
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
    ---@type Source.Data.TextStyleValues
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

---@param data  table
---@param name  string
---@param value table
---@return Engine.PlainTextConfig
local function buildPlainTextConfig(data, name, value)
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
        gradient = textGradientFromData(data, value.gradient, sourceName .. ".gradient")
    })
end

---@param data  table
---@param name  string
---@param value table
---@return Engine.RichTextConfig
local function buildRichTextConfig(data, name, value)
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
        gradient = textGradientFromData(data, value.gradient, sourceName .. ".gradient")
    })
end

function DataTextConfigs:getPlainTextConfig(name)
    local cached = self._data._plainTextConfigs[name]
    if cached ~= nil then
        return cached
    end
    local value = requireNamedValue(
        self._data._textConfigData, name, "Text config data not found: " .. tostring(name)
    )
    assert(value.type == "plainTextConfig", "Text config is not plain text: " .. tostring(name))
    cached = buildPlainTextConfig(self._data, name, value)
    self._data._plainTextConfigs[name] = cached
    return cached
end

function DataTextConfigs:getRichTextConfig(name)
    local cached = self._data._richTextConfigs[name]
    if cached ~= nil then
        return cached
    end
    local value = requireNamedValue(
        self._data._textConfigData, name, "Text config data not found: " .. tostring(name)
    )
    assert(value.type == "richTextConfig", "Text config is not rich text: " .. tostring(name))
    cached = buildRichTextConfig(self._data, name, value)
    self._data._richTextConfigs[name] = cached
    return cached
end

return class(DataTextConfigs)
