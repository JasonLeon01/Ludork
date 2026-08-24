local Engine = require("Engine")
local PlayerAttrHUDUI = require("Source.UI.PlayerAttrHUD")

local Canvas = Engine.Canvas

local PlayerAttrHUD = {}

PlayerAttrHUD._AVATAR_MIN_SIZE = 32
PlayerAttrHUD._FONT_SIZE = 18
PlayerAttrHUD._HUD_POS_X = 16
PlayerAttrHUD._HUD_POS_Y = 16
PlayerAttrHUD._STATE_ICON_SIZE = 16
PlayerAttrHUD._STATE_GAP = 4
PlayerAttrHUD._ROW_SHIFT = 32
PlayerAttrHUD._HEADER_ROW_Y = 0
PlayerAttrHUD._HP_ROW_Y = 68 + PlayerAttrHUD._ROW_SHIFT
PlayerAttrHUD._HP_BAR_OFFSET_Y = 8
PlayerAttrHUD._HP_BAR_HEIGHT = 8
PlayerAttrHUD._HP_TEXT_LAYOUT_HEIGHT = 12
PlayerAttrHUD._HP_BAR_WIDTH = 96
PlayerAttrHUD._STAT_VALUE_X = 128
PlayerAttrHUD._DEBUFF_TEXT_OFFSET_X = 2
PlayerAttrHUD._ATK_ROW_Y = 96 + PlayerAttrHUD._ROW_SHIFT
PlayerAttrHUD._DEF_ROW_Y = 128 + PlayerAttrHUD._ROW_SHIFT
PlayerAttrHUD._EXP_ROW_Y = 192 + PlayerAttrHUD._ROW_SHIFT
PlayerAttrHUD._GOLD_ROW_Y = 224 + PlayerAttrHUD._ROW_SHIFT
PlayerAttrHUD._KEY_ROW_Y = 288 + PlayerAttrHUD._ROW_SHIFT
PlayerAttrHUD._KEY_ICON_HEIGHT = 32

function PlayerAttrHUD:init(player, openMenuCallback)
    self._player = player
    self._openMenuCallback = openMenuCallback
    self._ui = PlayerAttrHUDUI.new(self)
    local logicalSize = self._ui:getLogicalSize()
    super(PlayerAttrHUD, self).init(Engine.ToIntRect(self._HUD_POS_X, self._HUD_POS_Y, logicalSize.x, logicalSize.y))
    self._ui:attach(logicalSize)
end

function PlayerAttrHUD:setPlayer(player)
    self._player = player
end

function PlayerAttrHUD:onTick(deltaTime)
    self:_refresh()
    return super(PlayerAttrHUD, self).onTick(deltaTime)
end

---@param iconPath string
---@return sf.Texture | nil
function PlayerAttrHUD:_loadStateIcon(iconPath)
    return self._ui:loadStateIcon(iconPath)
end

function PlayerAttrHUD:_clearStateWidgets()
    self._ui:clearStateRows()
end

---@param states table
---@return boolean
function PlayerAttrHUD:_stateSignatureMatches(states)
    return self._ui:stateSignatureMatches(states)
end

function PlayerAttrHUD:_refreshStates()
    self._ui:refreshStates()
end

---@return string
function PlayerAttrHUD:_getMapDisplayName()
    return self._ui:getMapDisplayName()
end

function PlayerAttrHUD:_refresh()
    self._ui:tick()
end

return class(PlayerAttrHUD, Canvas)
