---@meta Source.Windows.ConfigWindow.ConfigRow

---@class Source.Windows.ConfigWindow.ConfigRow: Source.UIBase.UiController
---@field root       Engine.Canvas
---@field _labelText string
local ConfigRowControllerBase = {}

---@param ui        Source.UIBase.UiView
---@param labelText string
function ConfigRowControllerBase:init(ui, labelText) end

---@return Engine.Canvas
function ConfigRowControllerBase:prepare() end

---@return boolean
function ConfigRowControllerBase:getActive() end

---@param active boolean
function ConfigRowControllerBase:setActive(active) end

---@param callback function
function ConfigRowControllerBase:addConfirmCallback(callback) end

---@param labelText string
function ConfigRowControllerBase:setLabelText(labelText) end

---@return table
function ConfigRowControllerBase:getChildren() end

---@return sf.Vector2f
function ConfigRowControllerBase:getSize() end

---@return sf.FloatRect
function ConfigRowControllerBase:getLocalBounds() end

---@param deltaTime number
function ConfigRowControllerBase:onTick(deltaTime) end

return ConfigRowControllerBase
