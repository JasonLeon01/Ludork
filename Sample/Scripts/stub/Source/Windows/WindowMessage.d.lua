---@meta Source.Windows.WindowMessage
---
--- Supports fade-in/out, speaker name display, multi-option selection,
--- and automatic positioning relative to reference actors.
---@class Source.Windows.WindowMessage: Source.Windows.Base.WindowSelectable
---@field new fun(): Source.Windows.WindowMessage
---@field _WINDOW_PADDING integer
---@field _SCREEN_EDGE_MARGIN integer
---@field _NAME_MESSAGE_GAP integer
---@field _OPTION_ITEM_HEIGHT integer
---@field _SELECTION_LIST_HORIZONTAL_INSET integer
---@field _TEXT_RENDER_GUTTER integer
---@field _MAX_OPTIONS integer
---@field _fadeCurves table<string, Engine.Curve>
---@field _inDialogue boolean
---@field _contentMode integer
---@field _selectionListView Engine.ListView | nil
---@field _messageListView Engine.ListView | nil
---@field _messageAdvancer Engine.FunctionalPlainText | nil
---@field _selectionResult integer | nil
---@field _allowCancel boolean
---@field _onFinished function | nil
---@field _fadePhase integer
---@field _fadeTime number
---@field _pendingLayout boolean
---@field _pendingRefPosition sf.Vector2f | nil
---@field _name string
---@field _message string
---@field _ui Source.UI.WindowMessage
---@field _nameText Engine.PlainText
---@field _text Engine.RichText
local WindowMessage = {}

--- @brief Construct a message window with default fade and layout settings.
function WindowMessage:init() end

--- @brief Update fade animations, layout, and selection cursor visibility.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowMessage:onTick(deltaTime) end

--- @brief Handle keyboard input for selection cancel and option navigation.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowMessage:onKeyDown(kwargs) end

--- @brief Cancel the current selection dialogue through the shared return path.
function WindowMessage:onReturn() end

--- @brief Advance a plain message dialogue on left mouse click anywhere within the window.
---
--- - @param kwargs Event data with cursor position.
---@param kwargs table
function WindowMessage:onClick(kwargs) end

--- @brief Check if the window is currently showing a dialogue.
---
--- - @return True if in dialogue mode.
---@return boolean
function WindowMessage:isInDialogue() end

--- @brief Check if a plain message dialogue can be advanced by confirm input.
---
--- - @return True when a non-option message is waiting for confirm.
---@return boolean
function WindowMessage:isAwaitingMessageConfirm() end

--- @brief Confirm the current plain message dialogue if it is waiting.
---
--- - @return True if the message was confirmed.
---@return boolean
function WindowMessage:confirmMessage() end

--- @brief Get the result of a selection dialogue.
---
--- - @return The selected option index, or nil if no selection has been made.
---@return integer | nil
function WindowMessage:getSelectionResult() end

--- @brief Show a message or selection dialogue.
---
--- - @param refPosition Optional reference position for window placement.
--- - @param name Speaker name to display.
--- - @param message Message text (str) or selection options (list).
--- - @param allowCancel Whether the selection can be cancelled.
--- - @param onFinished Optional callback invoked when the dialogue is confirmed/cancelled.
---@param refPosition sf.Vector2f | nil
---@param name        string
---@param message     string | table
---@param allowCancel boolean | nil
---@param onFinished  function | nil
function WindowMessage:setMessage(refPosition, name, message, allowCancel, onFinished) end

--- @brief Replace the visible dialogue text without resetting interaction or fade state.
---
--- Selection content must keep the same option count as the active dialogue. The window preserves
--- the current option, callbacks, focus, fade progress, completion state, and reference position.
---@param name    string
---@param message string | string[]
function WindowMessage:refreshContent(name, message) end

return WindowMessage
