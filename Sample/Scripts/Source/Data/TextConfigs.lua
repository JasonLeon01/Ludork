local Engine = require("Engine")
local Validation = require("Source.Data.Validation")

local requireNamedValue = Validation.RequireNamedValue
local TextConfig = Engine.TextConfig

local DataTextConfigs = {}

function DataTextConfigs:init(data)
    self._data = data
end

function DataTextConfigs:getPlainTextConfig(name)
    if self._data._plainTextConfigs[name] ~= nil then
        return self._data._plainTextConfigs[name]
    end
    local value = requireNamedValue(self._data._textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "plainTextConfig", "Text config is not plain text: " .. tostring(name))
    local cached = TextConfig.buildPlain(value, name)
    self._data._plainTextConfigs[name] = cached
    return cached
end

function DataTextConfigs:getRichTextConfig(name)
    if self._data._richTextConfigs[name] ~= nil then
        return self._data._richTextConfigs[name]
    end
    local value = requireNamedValue(self._data._textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "richTextConfig", "Text config is not rich text: " .. tostring(name))
    local cached = TextConfig.buildRich(value, name)
    self._data._richTextConfigs[name] = cached
    return cached
end

return class(DataTextConfigs)
