local Engine = require("Engine")
local PlayerAttrHUDUI = require("Source.UI.PlayerAttrHUD")

local Canvas = Engine.Canvas

local PlayerAttrHUD = {}

local _HUD_POS_X = 16
local _HUD_POS_Y = 16

function PlayerAttrHUD:init(player, openMenuCallback)
    self._player = player
    self._openMenuCallback = openMenuCallback
    self._ui = PlayerAttrHUDUI.new(self)
    local logicalSize = self._ui:getLogicalSize()
    super(PlayerAttrHUD, self).init(Engine.ToIntRect(_HUD_POS_X, _HUD_POS_Y, logicalSize.x, logicalSize.y))
    self._ui:attach(logicalSize)
end

function PlayerAttrHUD:setPlayer(player)
    self._player = player
end

function PlayerAttrHUD:onTick(deltaTime)
    self._ui:tick()
    return super(PlayerAttrHUD, self).onTick(deltaTime)
end

function PlayerAttrHUD:getPlayer()
    return self._player
end

function PlayerAttrHUD:openMenu()
    if self._openMenuCallback ~= nil then
        self._openMenuCallback()
    end
end

return class(PlayerAttrHUD, Canvas)
