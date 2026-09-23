---@meta Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller

---@class Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller.Model
---@field iconTexture? sf.Texture
---@field name         string

---@class Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller: Internal.UIBase.UiController
---@field ui              Internal.UI.Parts.PlayerAttrHUD.PlayerStateRow
---@field model           Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller.Model
---@field _logicalSize    sf.Vector2u
---@field _contentPadding number
---@field _iconSize       number
---@field _width          number
local PlayerStateRowController = {}

---@param model Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller.Model
---@return Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller
function PlayerStateRowController.new(model) end

function PlayerStateRowController:init(model) end

function PlayerStateRowController:bind() end

function PlayerStateRowController:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function PlayerStateRowController:prepare(logicalSize) end

---@return number
function PlayerStateRowController:getWidth() end

return PlayerStateRowController
