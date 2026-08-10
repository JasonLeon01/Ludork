---@meta Source.UI.Parts.ConfigWindow.ConfigRow

---@class Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase : Source.UI.UiController
local ConfigRowControllerBase = {}

---@param model     any
---@param rowWidth  integer
---@param rowHeight integer
function ConfigRowControllerBase:init(model, rowWidth, rowHeight) end

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

---@return sf.Vector2u
function ConfigRowControllerBase:getSize() end

---@return sf.FloatRect
function ConfigRowControllerBase:getLocalBounds() end

---@param deltaTime number
function ConfigRowControllerBase:onTick(deltaTime) end

---@param rowHeight number
function ConfigRowControllerBase:setRowHeight(rowHeight) end

return ConfigRowControllerBase
