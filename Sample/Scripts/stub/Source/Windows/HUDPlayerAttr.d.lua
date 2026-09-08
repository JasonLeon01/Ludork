---@meta Source.Windows.HUDPlayerAttr
---
--- Shows the player's avatar, current map name, level, states, HP bar with value, and stat values.
---@class Source.Windows.PlayerAttrHUD: Engine.Canvas
---@field new               fun(player: Source.Player.Player, openMenuCallback?: function): Source.Windows.PlayerAttrHUD
---@field _player           Source.Player.Player
---@field _openMenuCallback function | nil
---@field _ui               Source.UI.PlayerAttrHUD.PlayerAttrHUDUI
local PlayerAttrHUD = {}

--- Construct a player attribute HUD bound to the given player instance.
---
--- - @param player  Target player whose attributes are displayed on this HUD
--- - @param openMenuCallback Callback invoked when the player avatar is clicked
---@param player           Source.Player.Player
---@param openMenuCallback function | nil
function PlayerAttrHUD:init(player, openMenuCallback) end

--- Rebind the player whose values are displayed by this HUD.
---
--- - @param player Target player.
---@param player Source.Player.Player
function PlayerAttrHUD:setPlayer(player) end

--- Poll lightweight player signatures every frame and refresh only the HUD groups whose displayed values changed.
---
--- - @param deltaTime  Elapsed frame time in seconds
---@param deltaTime number
function PlayerAttrHUD:onTick(deltaTime) end

---@return Source.Player.Player
function PlayerAttrHUD:getPlayer() end

function PlayerAttrHUD:openMenu() end

return PlayerAttrHUD
