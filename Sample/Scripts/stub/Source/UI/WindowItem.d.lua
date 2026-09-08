---@meta Source.UI.WindowItem

---@class Source.UI.WindowItem: Source.UI.UiController
---@field model               Source.Windows.WindowItem
---@field _itemList           { [1]: string, [2]: integer } []
---@field _lastDescIndex      integer | nil
---@field _descMaxWidth       integer
---@field _logicalSize        sf.Vector2u | nil
---@field _rowUIs             Source.UI.Parts.WindowItem.ItemRow[]
---@field _windowFrame        Engine.Window
---@field _content            Engine.Canvas
---@field _scrollBox          Engine.ScrollBox
---@field _listView           Engine.ListView
---@field _descriptionControl Engine.PlainText
---@field new                 fun(model: Source.Windows.WindowItem): Source.UI.WindowItem
local WindowItemUI = {}

function WindowItemUI:init(model) end

function WindowItemUI:attach() end

function WindowItemUI:refresh() end

function WindowItemUI:refreshItems() end

function WindowItemUI:tick() end

---@param text string
---@return string
function WindowItemUI:wrapDescription(text) end

function WindowItemUI:updateDescription() end

function WindowItemUI:open() end

---@param onHidden function | nil
function WindowItemUI:close(onHidden) end

---@return Engine.Window
function WindowItemUI:getWindowFrame() end

---@return Engine.Canvas
function WindowItemUI:getContent() end

function WindowItemUI:useSelectedItem() end

function WindowItemUI:closeByCancel() end

return WindowItemUI
