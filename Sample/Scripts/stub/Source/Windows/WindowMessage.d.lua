---@meta

---
--- Supports Timeline fade-in/out, speaker name display, multi-option selection,
--- and automatic positioning relative to reference actors.
---@class Source.Windows.WindowMessage.Controller: Source.UIBase.UiController
---@field host                Source.Windows.WindowMessage
---@field _OPTION_ITEM_HEIGHT integer
---@field _MAX_OPTIONS        integer
---@field _inDialogue         boolean
---@field _contentMode        integer
---@field _selectionResult    integer | nil
---@field _allowCancel        boolean
---@field _onFinished         fun() | nil
---@field _pendingLayout      boolean
---@field _pendingFadeIn      boolean
---@field _pendingRefPosition sf.Vector2f | nil
---@field ui                  Source.UI.WindowMessage
---@field root                Engine.Canvas
---@field _messageAdvancer    Engine.FunctionalPlainText | nil
---@field _name               string
---@field _message            string
---@field _panelSize          sf.Vector2f
---@field _messageRows        Source.UIBase.UiCollection<Source.Windows.WindowMessage.MessageOptionRow.Controller>
---@field _selectionRows      Source.UIBase.UiCollection<Source.Windows.WindowMessage.MessageOptionRow.Controller>
local Controller = {}

---@brief Construct a message window with default fade and layout settings.
function Controller:init() end

---@brief Attach the current non-overflowing message or option list directly to the message content.
---@param listView Engine.ListView | nil
function Controller:setListView(listView) end

---@brief Update fade animations, layout, and selection cursor visibility.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function Controller:onTick(deltaTime) end

---@brief Handle keyboard input for selection cancel and option navigation.
---
--- - @param kwargs Event data.
---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

---@brief Cancel the current selection dialogue through the shared return path.
function Controller:onReturn() end

---@brief Check if the window is currently showing a dialogue.
---
--- - @return True if in dialogue mode.
---@return boolean
function Controller:isInDialogue() end

---@brief Check if a plain message dialogue can be advanced by confirm input.
---
--- - @return True when a non-option message is waiting for confirm.
---@return boolean
function Controller:isAwaitingMessageConfirm() end

---@brief Confirm the current plain message dialogue if it is waiting.
---
--- - @return True if the message was confirmed.
---@return boolean
function Controller:confirmMessage() end

---@brief Get the result of a selection dialogue.
---
--- - @return The selected option index, or nil if no selection has been made.
---@return integer | nil
function Controller:getSelectionResult() end

---@brief Show a message dialogue.
---
--- - @param refPosition Optional reference position for window placement.
--- - @param name Speaker name to display.
--- - @param message Message text.
--- - @param onFinished Optional callback invoked when the dialogue is confirmed/cancelled.
---@param refPosition sf.Vector2f | nil
---@param name        string
---@param message     string
---@param onFinished  fun() | nil
function Controller:setMessage(refPosition, name, message, onFinished) end

---@brief Show a selection dialogue.
---
--- - @param refPosition Optional reference position for window placement.
--- - @param name Speaker name to display.
--- - @param options Selection options.
--- - @param allowCancel Whether the selection can be cancelled.
--- - @param onFinished Optional callback invoked when the dialogue is confirmed/cancelled.
---@param refPosition sf.Vector2f | nil
---@param name        string
---@param options     string[]
---@param allowCancel boolean | nil
---@param onFinished  fun() | nil
function Controller:setSelection(refPosition, name, options, allowCancel, onFinished) end

---@brief Replace visible message text without resetting interaction or fade state.
---@param name    string
---@param message string
function Controller:refreshMessage(name, message) end

---@brief Replace visible selection text without resetting interaction or fade state.
---
--- The option count must match the active selection. The window preserves the current option,
--- callbacks, focus, fade progress, completion state, and reference position.
---@param name    string
---@param options string[]
function Controller:refreshSelection(name, options) end

function Controller:bind() end

function Controller:refresh() end

---@return Engine.Canvas
function Controller:prepare() end

---@param active boolean
function Controller:setConfirmLayerActive(active) end

---@param text string
function Controller:_setSpeakerName(text) end

---@param text string
function Controller:_setMessageText(text) end

function Controller:showMessageList() end

---@param options   string[]
---@param onConfirm function
---@param onCancel  function
---@return Engine.ListView
function Controller:showSelectionList(options, onConfirm, onCancel) end

function Controller:updateLayoutBySelectionSize() end

function Controller:updateLayoutByTextSize() end

---@param refPosition sf.Vector2f | nil
function Controller:updateWindowPosition(refPosition) end

function Controller:resetTextColour() end

---@param visible boolean
function Controller:setMessageVisible(visible) end

---@param index integer | nil
function Controller:cancelSelection(index) end

---@param options string[]
function Controller:_refreshSelectionText(options) end

---@param index     integer
---@param rowHeight number
---@return sf.Vector2f | nil
function Controller:getSelectionPosition(index, rowHeight) end

---@return integer | nil
function Controller:getSelectionWidth() end
