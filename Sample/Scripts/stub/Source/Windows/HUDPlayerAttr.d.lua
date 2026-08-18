---@meta Source.Windows.HUDPlayerAttr
---
--- Shows the player's avatar, current map name, level, states, HP bar with value, and stat values.
---@class Source.Windows.PlayerAttrHUD: Engine.Canvas
---@field new fun(player: Source.Player.Player, openMenuCallback?: function): Source.Windows.PlayerAttrHUD
---@field _player Source.Player.Player
---@field _openMenuCallback function | nil
---@field _ui Source.UI.PlayerAttrHUD.PlayerAttrHUDUI
---@field _AVATAR_MIN_SIZE integer
---@field _FONT_SIZE integer
---@field _HUD_POS_X integer
---@field _HUD_POS_Y integer
---@field _STATE_ICON_SIZE integer
---@field _STATE_GAP integer
---@field _ROW_SHIFT integer
---@field _HEADER_ROW_Y integer
---@field _HP_ROW_Y integer
---@field _HP_BAR_OFFSET_Y integer
---@field _HP_BAR_HEIGHT integer
---@field _HP_TEXT_LAYOUT_HEIGHT integer
---@field _HP_BAR_WIDTH integer
---@field _STAT_VALUE_X integer
---@field _DEBUFF_TEXT_OFFSET_X integer
---@field _ATK_ROW_Y integer
---@field _DEF_ROW_Y integer
---@field _EXP_ROW_Y integer
---@field _GOLD_ROW_Y integer
---@field _KEY_ROW_Y integer
---@field _KEY_ICON_HEIGHT integer
---@field _avatar Engine.Button | nil
---@field _mapNameText Engine.PlainText
---@field _levelText Engine.PlainText
---@field _hpBack Engine.SolidRect
---@field _hpFill Engine.SolidRect
---@field _hpLabelText Engine.PlainText
---@field _hpText Engine.RichText
---@field _statValueTexts table<string, Engine.PlainText>
---@field _atkDebuffText Engine.PlainText
---@field _defDebuffText Engine.PlainText
---@field _hpPoisonText Engine.PlainText
---@field _keyIcon Engine.Image
---@field _itemText Engine.RichText
---@field _stateWidgets Engine.Canvas[]
---@field _stateSignature tuple<string> | nil
---@field _stateIconCache table<string, sf.Texture>
local PlayerAttrHUD = {}

--- Construct a player attribute HUD bound to the given player instance.
---
--- - @param player  Target player whose attributes are displayed on this HUD
--- - @param openMenuCallback Callback invoked when the player avatar is clicked
---@param player           Source.Player.Player
---@param openMenuCallback function | nil
function PlayerAttrHUD:init(player, openMenuCallback) end

--- Poll lightweight player signatures every frame and refresh only the HUD groups whose displayed values changed.
---
--- - @param deltaTime  Elapsed frame time in seconds
---@param deltaTime number
function PlayerAttrHUD:onTick(deltaTime) end

return PlayerAttrHUD
