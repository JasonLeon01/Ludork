---@meta Global.Gameplay.AttributeSet

---@class Global.Gameplay.AttributeSchema
---@field type    string
---@field default any

---@class Global.Gameplay.AttributeSet
---@field ID              string
---@field ATTRIBUTE_NAMES string[]
---@field SCHEMA          table<string, Global.Gameplay.AttributeSchema>
---@field new             fun(values?: table<string, any>): Global.Gameplay.AttributeSet
local AttributeSet = {}

---@param values? table<string, any>
function AttributeSet:init(values) end

---@return string[]
function AttributeSet:getAttributeNames() end

---@param name string
---@return Global.Gameplay.AttributeSchema | nil
function AttributeSet:getAttributeSchema(name) end

return AttributeSet
