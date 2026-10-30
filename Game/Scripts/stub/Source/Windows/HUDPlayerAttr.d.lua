---@meta

---@alias Source.Windows.PlayerAttrHUD.ShortcutKey "enemyBook" | "floorTeleporter" | "shopRecords"

---@class Source.Windows.PlayerAttrHUD.ShortcutDefinition
---@field action Source.Windows.PlayerAttrHUD.ShortcutKey
---@field items  string[] | nil
---@field icon   string | nil

---
--- Shows the player's avatar, current map name, level, states, HP bar with value, and stat values.
---@class Source.Windows.PlayerAttrHUD.Controller: Internal.UIBase.UiController
---@field host                   Source.Windows.PlayerAttrHUD
---@field _inst                  Source.GameInstance.GameInstance
---@field _player                Source.MapActors.Player.Player
---@field _shortcutCallbacks     table<Source.Windows.PlayerAttrHUD.ShortcutKey, fun(): boolean>
---@field _shortcutSignature     string | nil
---@field _openMenuCallback      function | nil
---@field ui                     Internal.UI.PlayerAttrHUD
---@field refreshEvents          string[]
---@field _stateSignature        tuple<string> | nil
---@field _stateDisplaySignature tuple<string> | nil
---@field _avatarTexture         sf.Texture | nil
---@field _avatarRect            sf.IntRect | nil
---@field _avatarOffset          number
---@field _language              string
---@field _headerSignature       tuple<any> | nil
---@field _combatSignature       tuple<any> | nil
---@field _hpSignature           tuple<any> | nil
---@field _statSignature         tuple<any> | nil
---@field _stackSignature        tuple<any> | nil
---@field _progressSignature     tuple<any> | nil
---@field _keySignature          tuple<any> | nil
---@field _states                Internal.UIBase.UiCollection<Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller>
local Controller = {}

--- Construct the player HUD and its optional shortcut actions.
---@param inst              Source.GameInstance.GameInstance
---@param openMenuCallback  function | nil
---@param shortcutCallbacks table<Source.Windows.PlayerAttrHUD.ShortcutKey, fun(): boolean> | nil
function Controller:init(inst, openMenuCallback, shortcutCallbacks) end

--- Rebind the game instance and its primary player, including after loading a save.
---@param inst Source.GameInstance.GameInstance
function Controller:setInstance(inst) end

--- Ignore Ability System and player events from other battlers, then refresh the HUD.
---
--- - @param payload EventBus payload. Locale events have no owner; player and ability events include `owner`.
---@param payload Source.Configs.EventChangePayload | { language: string } | nil
function Controller:refreshFromEvent(payload) end

---@param payload Source.Configs.EventChangePayload | { language: string } | nil
function Controller:onViewUpdate(payload) end

---@return Source.MapActors.Player.Player
function Controller:getPlayer() end

function Controller:openMenu() end

---@param action Source.Windows.PlayerAttrHUD.ShortcutKey
function Controller:openShortcut(action) end

---@param payload Source.Configs.ShopRecordsChangePayload
function Controller:_onShopRecordsChanged(payload) end

--- Rebuild the native shortcut templates only when the available actions change.
function Controller:refreshShortcuts() end

function Controller:bind() end

---@param language string | nil
---@return boolean
function Controller:refreshStates(language) end

function Controller:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function Controller:prepare(logicalSize) end

function Controller:dispose() end
