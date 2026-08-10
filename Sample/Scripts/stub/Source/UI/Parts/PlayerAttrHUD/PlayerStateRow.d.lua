---@meta Source.UI.Parts.PlayerAttrHUD.PlayerStateRow

---@class Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.Model
---@field iconSize integer
---@field iconTexture? sf.Texture
---@field name string

---@class Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.PlayerStateRowUI : Source.UI.UiController
---@field model Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.Model
---@field _icon Engine.Image
---@field _nameText Engine.PlainText
local PlayerStateRowUI = {}

---@param model Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.Model
---@return Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.PlayerStateRowUI
function PlayerStateRowUI.new(model) end

function PlayerStateRowUI:init(model) end

function PlayerStateRowUI:bind() end

function PlayerStateRowUI:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function PlayerStateRowUI:prepare(logicalSize) end

---@return number
function PlayerStateRowUI:getWidth() end

return PlayerStateRowUI
