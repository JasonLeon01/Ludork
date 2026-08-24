---@meta Source.UI.Ui

---@class Source.UI.Ui.Module
local Ui = {}

---@param assetKey string
---@return string
function Ui.GetEventName(assetKey) end

---@param assetKey string
---@param payload  any
function Ui.Publish(assetKey, payload) end

---@generic T: table
---@param assetKey   string
---@param definition T
---@param baseClass  Class.ClassType<any> | table | nil
---@return T & Class.ClassType<T>
function Ui.Define(assetKey, definition, baseClass) end

return Ui
