local MessageOptionRowUI = require("Source.UI.Parts.WindowMessage.MessageOptionRow")
local Ui = require("Source.UI.Ui")

local _WINDOW_WIDTH = 544
local _WINDOW_HEIGHT = 160
local _OPTION_ITEM_HEIGHT = 32

---@class Source.UI.WindowMessage
local WindowMessageUI = {}

function WindowMessageUI:init(model)
    self._messageAdvancer = nil
    self._messageAdvancerUI = nil
    self._selectionRowUIs = {}
    super(WindowMessageUI, self).init(model, nil)
end

function WindowMessageUI:bind()
    local confirmLayer = self:requireControl("ConfirmLayer")
    ---@cast confirmLayer Engine.FunctionalImage & Engine.FunctionalBase
    self._confirmLayer = confirmLayer
    self._panel = self:requireControl("Panel")
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._nameText = self:requireControl("NameText")
    self._messageText = self:requireControl("MessageText")
    self._messageListView = self:requireControl("MessageList")
    self._selectionListView = self:requireControl("SelectionList")
    ---@cast self._messageListView Engine.ListView
    ---@cast self._selectionListView Engine.ListView
    self._messageListView:clearChildren()
    self._selectionListView:clearChildren()
    self._selectionListView:setVisible(false)
    self._confirmLayer:addMouseButtonDownCallback(function (_layer, kwargs)
        if kwargs.button ~= sf.Mouse.Button.Left or not self.model:isAwaitingMessageConfirm() then
            return false
        end
        return self.model:confirmMessage()
    end)
    self._confirmLayer:addConfirmCallback(function ()
        self.model:confirmMessage()
    end)
    self:setConfirmLayerActive(false)
end

function WindowMessageUI:refresh()
    self:setName(self.model._name)
    self:setMessage(self.model._message)
end

function WindowMessageUI:prepare()
    local size = self.model:getSize()
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    return super(WindowMessageUI, self).prepare(logicalSize)
end

function WindowMessageUI:attach()
    self:attachWindowView(self.model)
end

function WindowMessageUI:getWindowFrame()
    return self._windowFrame
end

function WindowMessageUI:getContent()
    return self._content
end

---@param active boolean
function WindowMessageUI:setConfirmLayerActive(active)
    self._confirmLayer:setActive(active)
    self._confirmLayer:setVisible(active)
end

---@param position sf.Vector2f
function WindowMessageUI:setPanelPosition(position)
    self._panel:setPosition(position)
end

function WindowMessageUI:getNameText()
    return self._nameText
end

function WindowMessageUI:getMessageText()
    return self._messageText
end

function WindowMessageUI:getMessageListView()
    return self._messageListView
end

function WindowMessageUI:getSelectionListView()
    return self._selectionListView
end

function WindowMessageUI:setName(text)
    self:setText("NameText", text)
end

function WindowMessageUI:setMessage(text)
    self:setText("MessageText", text)
end

function WindowMessageUI:attachMessageAdvancer(onConfirm)
    self._messageAdvancerUI = MessageOptionRowUI.new({
        text = "",
        onConfirm = onConfirm
    })
    local root = self._messageAdvancerUI:prepare()
    self._messageAdvancer = root
    root:setVisible(false)
    self._messageListView:addChild(root)
    return root, self._messageListView
end

function WindowMessageUI:showMessageList()
    self._selectionListView:setVisible(false)
    self.model:setListView(self._messageListView)
end

function WindowMessageUI:showSelectionList(options, onConfirm, onCancel)
    for _, rowUI in ipairs(self._selectionRowUIs) do
        rowUI:dispose()
    end
    self._selectionRowUIs = {}
    self._selectionListView:clearChildren()
    for luaIndex, optionText in ipairs(options) do
        local optionIndex = luaIndex - 1
        local rowUI = MessageOptionRowUI.new({
            text = optionText,
            onConfirm = function (_itemSelf, _kwargs)
                onConfirm(optionIndex)
            end,
            onCancel = function (_itemSelf, _kwargs)
                onCancel()
            end
        })
        local root = rowUI:prepare()
        self._selectionRowUIs[#self._selectionRowUIs + 1] = rowUI
        self.model:_applyItem(root)
        self._selectionListView:addChild(root)
    end
    self._selectionListView:setVisible(true)
    self.model:setListView(self._selectionListView)
    return self._selectionListView
end

function WindowMessageUI:reflow(width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    self._panel:resize(logicalSize)
    self._panel:setView(self._panel:getDefaultView())
    self._windowFrame:resize(logicalSize)
end

function WindowMessageUI:dispose()
    if self._confirmLayer ~= nil then
        self._confirmLayer:addMouseButtonDownCallback(function ()
            return false
        end)
        self._confirmLayer:addConfirmCallback(function ()
        end)
    end
    if self._messageAdvancerUI ~= nil then
        self._messageAdvancerUI:dispose()
        self._messageAdvancerUI = nil
    end
    for _, rowUI in ipairs(self._selectionRowUIs) do
        rowUI:dispose()
    end
    self._selectionRowUIs = {}
    super(WindowMessageUI, self).dispose()
end

return Ui.Define("WindowMessage", WindowMessageUI)
