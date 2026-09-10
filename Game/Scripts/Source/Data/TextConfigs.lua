local Engine = require("Engine")
local Validation = require("Source.Data.Validation")

local requireNamedValue = Validation.RequireNamedValue
local TextConfig = Engine.TextConfig

---@class Source.Data.TextConfigs
local DataTextConfigs = {}

---@param data Source.Data.Cache
function DataTextConfigs:init(data)
    self._state = data
end

function DataTextConfigs:getPlainTextConfig(name)
    if self._state.plainTextConfigs[name] ~= nil then
        return self._state.plainTextConfigs[name]
    end
    local value = requireNamedValue(self._state.textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "plainTextConfig", "Text config is not plain text: " .. tostring(name))
    local cached = TextConfig.buildPlain(value, name)
    self._state.plainTextConfigs[name] = cached
    return cached
end

function DataTextConfigs:getRichTextConfig(name)
    if self._state.richTextConfigs[name] ~= nil then
        return self._state.richTextConfigs[name]
    end
    local value = requireNamedValue(self._state.textConfigData, name, "Text config data not found: " .. tostring(name))
    assert(value.type == "richTextConfig", "Text config is not rich text: " .. tostring(name))
    local cached = TextConfig.buildRich(value, name)
    self._state.richTextConfigs[name] = cached
    return cached
end

return class(DataTextConfigs)
