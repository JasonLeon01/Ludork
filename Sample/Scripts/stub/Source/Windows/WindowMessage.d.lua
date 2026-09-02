---@meta Source.Windows.WindowMessage
---
--- Supports fade-in/out, speaker name display, multi-option selection,
--- and automatic positioning relative to reference actors.
---@class Source.Windows.WindowMessage: Source.Windows.Base.WindowSelectable
---@field new                              fun(): Source.Windows.WindowMessage
---@field _WINDOW_PADDING                  integer
---@field _SCREEN_EDGE_MARGIN              integer
---@field _NAME_MESSAGE_GAP                integer
---@field _OPTION_ITEM_HEIGHT              integer
---@field _SELECTION_LIST_HORIZONTAL_INSET integer
---@field _TEXT_RENDER_GUTTER              integer
---@field _MAX_OPTIONS                     integer
---@field _fadeCurves                      table<string, Engine.Curve>
---@field _inDialogue                      boolean
---@field _contentMode                     integer
---@field _selectionListView               Engine.ListView | nil
---@field _messageListView                 Engine.ListView | nil
---@field _messageAdvancer                 Engine.FunctionalPlainText | nil
---@field _selectionResult                 integer | nil
---@field _allowCancel                     boolean
---@field _onFinished                      fun() | nil
---@field _fadePhase                       integer
---@field _fadeTime                        number
---@field _pendingLayout                   boolean
---@field _pendingRefPosition              sf.Vector2f | nil
---@field _name                            string
---@field _message                         string
---@field _panelSize                       sf.Vector2f
---@field _ui                              Source.UI.WindowMessage
---@field _nameText                        Engine.PlainText
---@field _text                            Engine.RichText
local WindowMessage = {}

---@brief Construct a message window with default fade and layout settings.
function WindowMessage:init() end

---@brief Attach the current non-overflowing message or option list directly to the message content.
---@param listView Engine.ListView | nil
function WindowMessage:setListView(listView) end

---@brief Update fade animations, layout, and selection cursor visibility.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowMessage:onTick(deltaTime) end

---@brief Handle keyboard input for selection cancel and option navigation.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowMessage:onKeyDown(kwargs) end

---@brief Cancel the current selection dialogue through the shared return path.
function WindowMessage:onReturn() end

---@brief Check if the window is currently showing a dialogue.
---
--- - @return True if in dialogue mode.
---@return boolean
function WindowMessage:isInDialogue() end

---@brief Check if a plain message dialogue can be advanced by confirm input.
---
--- - @return True when a non-option message is waiting for confirm.
---@return boolean
function WindowMessage:isAwaitingMessageConfirm() end

---@brief Confirm the current plain message dialogue if it is waiting.
---
--- - @return True if the message was confirmed.
---@return boolean
function WindowMessage:confirmMessage() end

---@brief Get the result of a selection dialogue.
---
--- - @return The selected option index, or nil if no selection has been made.
---@return integer | nil
function WindowMessage:getSelectionResult() end

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
function WindowMessage:setMessage(refPosition, name, message, onFinished) end

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
function WindowMessage:setSelection(refPosition, name, options, allowCancel, onFinished) end

---@brief Replace visible message text without resetting interaction or fade state.
---@param name    string
---@param message string
function WindowMessage:refreshMessage(name, message) end

---@brief Replace visible selection text without resetting interaction or fade state.
---
--- The option count must match the active selection. The window preserves the current option,
--- callbacks, focus, fade progress, completion state, and reference position.
---@param name    string
---@param options string[]
function WindowMessage:refreshSelection(name, options) end

return WindowMessage
