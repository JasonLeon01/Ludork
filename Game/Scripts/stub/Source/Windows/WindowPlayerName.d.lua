---@meta

---@class Source.Windows.WindowPlayerName.Controller: Source.UIBase.UiController
---@field host      Source.Windows.WindowPlayerName
---@field _player   Source.Player.Player
---@field _onClose  fun()
---@field ui        Source.UI.WindowPlayerName
---@field _errorKey string
local Controller = {}

---@param player  Source.Player.Player
---@param onClose fun()
function Controller:init(player, onClose) end

---@return Source.Player.Player
function Controller:getPlayer() end

---@param player Source.Player.Player
function Controller:setPlayer(player) end

---@brief Start a fresh draft with an initially inactive text editor.
function Controller:open() end

---@brief Discard input editing state and finish the closing animation before notifying the owner.
function Controller:close() end

function Controller:onReturn() end

---@brief Validate the draft and change the player name only on success.
---
--- Commits only a Unicode-trimmed, non-empty draft of at most 32 graphemes.
function Controller:confirm() end

---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

---@param kwargs Engine.UiInputEventArguments
---@return boolean
function Controller:onMouseButtonDown(kwargs) end

---@return Engine.FunctionalBase[]
function Controller:getFocusControls() end

function Controller:refreshLocale() end

function Controller:dispose() end

---@param text string
---@return boolean
function Controller:validateDraft(text) end

---@brief Close without changing the player name.
function Controller:cancel() end

function Controller:bind() end

function Controller:refresh() end

---@param key string
function Controller:setError(key) end

---@param enabled boolean
function Controller:setConfirmEnabled(enabled) end

---@param delta integer
function Controller:moveFocus(delta) end

---@brief Navigate with keyboard or gamepad; A activates editing even while Confirm has focus.
---@param control Engine.FunctionalBase
function Controller:handleKeyDown(control) end

---@param text string
function Controller:setDraft(text) end

function Controller:beginEditing() end

---@return string
function Controller:finishEditing() end

function Controller:cancelEditing() end

---@param key string | nil
function Controller:refreshError(key) end
