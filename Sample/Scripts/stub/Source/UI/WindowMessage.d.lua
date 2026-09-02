---@meta Source.UI.WindowMessage

---@class Source.UI.WindowMessage: Source.UI.UiController
---@field model              Source.Windows.WindowMessage
---@field root               Engine.Canvas
---@field _messageAdvancer   Engine.FunctionalPlainText | nil
---@field _messageAdvancerUI Source.UI.Parts.WindowMessage.MessageOptionRow | nil
---@field _selectionRowUIs   Source.UI.Parts.WindowMessage.MessageOptionRow[]
---@field _messageListView   Engine.ListView
---@field _selectionListView Engine.ListView
---@field _confirmLayer      Engine.FunctionalImage & Engine.FunctionalBase
---@field _panel             Engine.Canvas
---@field _windowFrame       Engine.Window
---@field _content           Engine.Canvas
---@field _nameText          Engine.PlainText
---@field _messageText       Engine.RichText
---@field new                fun(model: Source.Windows.WindowMessage): Source.UI.WindowMessage
local WindowMessageUI = {}

---@param model Source.Windows.WindowMessage
function WindowMessageUI:init(model) end

function WindowMessageUI:bind() end

function WindowMessageUI:refresh() end

---@return Engine.Canvas
function WindowMessageUI:prepare() end

function WindowMessageUI:attach() end

---@return Engine.Window
function WindowMessageUI:getWindowFrame() end

---@return Engine.Canvas
function WindowMessageUI:getContent() end

---@param active boolean
function WindowMessageUI:setConfirmLayerActive(active) end

---@param position sf.Vector2f
function WindowMessageUI:setPanelPosition(position) end

---@return Engine.PlainText
function WindowMessageUI:getNameText() end

---@return Engine.RichText
function WindowMessageUI:getMessageText() end

---@return Engine.ListView
function WindowMessageUI:getMessageListView() end

---@return Engine.ListView
function WindowMessageUI:getSelectionListView() end

---@param text string
function WindowMessageUI:setName(text) end

---@param text string
function WindowMessageUI:setMessage(text) end

---@param onConfirm function
---@return Engine.FunctionalPlainText, Engine.ListView
function WindowMessageUI:attachMessageAdvancer(onConfirm) end

function WindowMessageUI:showMessageList() end

---@param options   string[]
---@param onConfirm function
---@param onCancel  function
---@return Engine.ListView
function WindowMessageUI:showSelectionList(options, onConfirm, onCancel) end

---@param width  integer
---@param height integer
function WindowMessageUI:reflow(width, height) end

function WindowMessageUI:dispose() end

return WindowMessageUI
