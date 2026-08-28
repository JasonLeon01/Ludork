local TextConfigParser = require("Source.Data.TextConfigParser")
local Validation = require("Source.Data.Validation")

local requireNamedValue = Validation.RequireNamedValue

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
    local cached = TextConfigParser.BuildPlain(self._data, name, value)
    self._data._plainTextConfigs[name] = cached
    return cached
end

function DataTextConfigs:getRichTextConfig(name)
    if self._data._richTextConfigs[name] ~= nil then
        return self._data._richTextConfigs[name]
    end
    local value = requireNamedValue(self._data._textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "richTextConfig", "Text config is not rich text: " .. tostring(name))
    local cached = TextConfigParser.BuildRich(self._data, name, value)
    self._data._richTextConfigs[name] = cached
    return cached
end

return class(DataTextConfigs)
