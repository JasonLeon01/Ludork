local Data = require("Source.Data")
local Ui = require("Source.UI.Ui")
local UiControlFactory = require("Source.UI.UiControlFactory")
local ListViewController = require("Source.UI.Helpers.ListView")

local _WINDOW_WIDTH = 544
local _WINDOW_HEIGHT = 160
local _OPTION_ITEM_HEIGHT = 32
local _OPTION_TEXT_CONFIG = "UI/Default"

---@class Source.UI.WindowMessage
local WindowMessageUI = {}

local function createOptionText(text)
    local control = UiControlFactory.createFunctionalPlainText(Data.getPlainTextConfig(_OPTION_TEXT_CONFIG))
    control:setString(text)
    return control
end

function WindowMessageUI:init(model)
    self._messageAdvancer = nil
    super(WindowMessageUI, self).init(model, nil)
    local messageListSize = sf.Vector2u.new(1, 1)
    ---@cast messageListSize sf.Vector2u
    self._messageListController = ListViewController.new(model, messageListSize, _OPTION_ITEM_HEIGHT, true, 1)
    local selectionListSize = sf.Vector2u.new(1, 1)
    ---@cast selectionListSize sf.Vector2u
    self._selectionListController = ListViewController.new(model, selectionListSize, _OPTION_ITEM_HEIGHT, true, 1)
    self._messageListView = self._messageListController:prepare()
    self._selectionListView = self._selectionListController:prepare()
end

function WindowMessageUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._nameText = self:requireControl("NameText")
    self._messageText = self:requireControl("MessageText")
end

function WindowMessageUI:refresh()
    self:setName(self.model._name)
    self:setMessage(self.model._message)
end

function WindowMessageUI:prepare()
    return super(WindowMessageUI, self).prepare(sf.Vector2u.new(_WINDOW_WIDTH, _WINDOW_HEIGHT))
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
    local root = createOptionText("")
    self._messageAdvancer = root
    root:setVisible(false)
    root:addConfirmCallback(onConfirm)
    self._messageListView:addChild(root)
    return root, self._messageListView
end

function WindowMessageUI:showMessageList()
    self.model:setListView(self._messageListView)
end

function WindowMessageUI:showSelectionList(options, onConfirm, onCancel)
    self._selectionListView:clearChildren()
    for luaIndex, optionText in ipairs(options) do
        local optionIndex = luaIndex - 1
        local root = createOptionText(optionText)
        root:addConfirmCallback(function (_itemSelf, _kwargs)
            onConfirm(optionIndex)
        end)
        root:addCancelCallback(function (_itemSelf, _kwargs)
            onCancel()
        end)
        self.model:_applyItem(root)
        self._selectionListView:addChild(root)
    end
    self.model:setListView(self._selectionListView)
    return self._selectionListView
end

function WindowMessageUI:reflow(width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    self.view:reflow(logicalSize)
    self.root:setView(self.root:getDefaultView())
end

return Ui.define("WindowMessage", WindowMessageUI)
