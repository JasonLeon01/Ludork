---@meta Source.UI.PlayerAttrHUD

---@class Source.UI.PlayerAttrHUD.PlayerAttrHUDUI : Source.UI.UiController
---@field model Source.Windows.PlayerAttrHUD
---@field _stateUIs Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.PlayerStateRowUI[]
---@field _stateWidgets Engine.Canvas[]
---@field _stateSignature tuple<string> | nil
---@field _stateIconCache table<string, sf.Texture>
---@field _avatarTexture sf.Texture | nil
---@field _avatarRect sf.IntRect | nil
---@field _avatarSize integer
---@field _infoStartX integer
---@field _hpBarWidth integer
---@field _logicalSize sf.Vector2u
---@field _hpRate number
---@field _avatar Engine.Button
---@field _mapNameText Engine.PlainText
---@field _levelText Engine.PlainText
---@field _stateHost Engine.Canvas
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
local PlayerAttrHUDUI = {}

---@return Source.UI.PlayerAttrHUD.PlayerAttrHUDUI
function PlayerAttrHUDUI.new(...) end

function PlayerAttrHUDUI:init(model) end

function PlayerAttrHUDUI:bind() end

---@return sf.Vector2u
function PlayerAttrHUDUI:getLogicalSize() end

---@param iconPath string
---@return sf.Texture | nil
function PlayerAttrHUDUI:loadStateIcon(iconPath) end

function PlayerAttrHUDUI:clearStateRows() end

---@param states table
---@return boolean
function PlayerAttrHUDUI:stateSignatureMatches(states) end

function PlayerAttrHUDUI:refreshStates() end

---@param mapName string
---@return string
function PlayerAttrHUDUI.formatMapName(mapName) end

---@return string
function PlayerAttrHUDUI:getMapDisplayName() end

function PlayerAttrHUDUI:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function PlayerAttrHUDUI:prepare(logicalSize) end

---@param logicalSize sf.Vector2u
function PlayerAttrHUDUI:attach(logicalSize) end

function PlayerAttrHUDUI:tick() end

return PlayerAttrHUDUI
