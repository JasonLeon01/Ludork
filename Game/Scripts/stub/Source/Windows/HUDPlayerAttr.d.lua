---@meta

---
--- Shows the player's avatar, current map name, level, states, HP bar with value, and stat values.
---@class Source.Windows.PlayerAttrHUD.Controller: Source.UIBase.UiController
---@field host                   Source.Windows.PlayerAttrHUD
---@field _player                Source.Player.Player
---@field _openMenuCallback      function | nil
---@field ui                     Source.UI.PlayerAttrHUD
---@field refreshEvents          string[]
---@field _stateSignature        tuple<string> | nil
---@field _stateDisplaySignature tuple<string> | nil
---@field _avatarTexture         sf.Texture | nil
---@field _avatarRect            sf.IntRect | nil
---@field _avatarSize            integer
---@field _infoStartX            integer
---@field _language              string
---@field _headerSignature       tuple<any> | nil
---@field _combatSignature       tuple<any> | nil
---@field _hpSignature           tuple<any> | nil
---@field _statSignature         tuple<any> | nil
---@field _stackSignature        tuple<any> | nil
---@field _progressSignature     tuple<any> | nil
---@field _keySignature          tuple<any> | nil
---@field _states                Source.UIBase.UiCollection<Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller>
local Controller = {}

--- Construct a player attribute HUD bound to the given player instance.
---
--- - @param player  Target player whose attributes are displayed on this HUD
--- - @param openMenuCallback Callback invoked when the player avatar is clicked
---@param player           Source.Player.Player
---@param openMenuCallback function | nil
function Controller:init(player, openMenuCallback) end

--- Rebind the player whose values are displayed by this HUD.
---
--- - @param player Target player.
---@param player Source.Player.Player
function Controller:setPlayer(player) end

--- Ignore Ability System and player events from other battlers, then refresh the HUD.
---
--- - @param payload EventBus payload. Locale events have no owner; player and ability events include `owner`.
---@param payload Source.Configs.EventKeys.ChangePayload | { language: string } | nil
function Controller:refreshFromEvent(payload) end

---@return Source.Player.Player
function Controller:getPlayer() end

function Controller:openMenu() end

function Controller:bind() end

---@param language string | nil
---@return boolean
function Controller:refreshStates(language) end

function Controller:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function Controller:prepare(logicalSize) end

function Controller:dispose() end
