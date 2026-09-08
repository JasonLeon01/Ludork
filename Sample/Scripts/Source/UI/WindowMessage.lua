local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local WindowMessageLayout = require("Source.UI.WindowMessage.Layout")
local MessageOptionRowUI = require("Source.UI.Parts.WindowMessage.MessageOptionRow")
local Ui = require("Source.UI.Ui")

local PlainText = Engine.PlainText
local RichText = Engine.RichText
local TextLayout = Engine.TextLayout
local GlobalSystem = GlobalCore.System
local _MESSAGE_TEXT_CONFIG = "UI/Message"

local _WINDOW_WIDTH = 544
local _WINDOW_HEIGHT = 160
local _OPTION_ITEM_HEIGHT = 32

local _WINDOW_PADDING = 16

local _SCREEN_EDGE_MARGIN = 64

local _NAME_MESSAGE_GAP = 8

local _SELECTION_LIST_HORIZONTAL_INSET = 32

local _TEXT_RENDER_GUTTER = 2

---@class Source.UI.WindowMessage
local WindowMessageUI = {}

function WindowMessageUI:init(model)
    self._name = ""
    self._message = ""
    self._panelSize = sf.Vector2f.new(544.0, 160.0)
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
    ---@cast self._windowFrame Engine.Window
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
    self._windowFrame:setColour(sf.Color.new(255, 255, 255, 192))
    self._nameText:setVisible(false)
end

function WindowMessageUI:refresh()
    self:setName(self._name)
    self:setMessage(self._message)
end

function WindowMessageUI:prepare()
    local size = self.model:getSize()
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    return super(WindowMessageUI, self).prepare(logicalSize)
end

function WindowMessageUI:attach()
    self:attachWindowView(self.model, nil, "Panel")
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
    self._name = text
    self._nameText:setVisible(text:match("%S") ~= nil)
    self:setText("NameText", text)
end

function WindowMessageUI:setMessage(text)
    self._message = text
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
        ---@cast root Engine.FunctionalPlainText
        root:setColour(sf.Color.White)
        self._selectionRowUIs[#self._selectionRowUIs + 1] = rowUI
        self.model:applyItem(root)
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

function WindowMessageUI:updateWindowPosition(refPosition)
    local gameSize = GlobalSystem.getGameSize()
    local gameWidth = gameSize.x + 0.0
    local gameHeight = gameSize.y + 0.0
    if refPosition == nil then
        local posX = (gameWidth - self._panelSize.x) / 2.0
        local posY = (gameHeight - self._panelSize.y) / 2.0
        self:setPanelPosition(sf.Vector2f.new(posX, posY))
    else
        local cellSize = Engine.GetCellSize() + 0.0
        local anchorX = refPosition.x + cellSize * 0.5
        local halfScreenY = gameHeight * 0.5
        local posY = nil
        if refPosition.y < halfScreenY then
            posY = refPosition.y + cellSize
        else
            posY = refPosition.y - self._panelSize.y
        end
        local posX = anchorX - self._panelSize.x * 0.5
        local maxX = math.max(0.0, gameWidth - self._panelSize.x)
        local maxY = math.max(0.0, gameHeight - self._panelSize.y)
        posX = math.clamp(posX, 0.0, maxX)
        posY = math.clamp(posY, 0.0, maxY)
        self:setPanelPosition(sf.Vector2f.new(posX, posY))
    end
end

local function getMaxWindowWidth()
    local gameWidth = GlobalSystem.getGameSize().x
    return math.max(1, gameWidth - _SCREEN_EDGE_MARGIN)
end

local function getTextRenderWidth(measuredWidth)
    return math.max(1, math.ceil(measuredWidth) + _TEXT_RENDER_GUTTER * 2)
end

local function getBoundsHeight(bounds)
    return math.max(1, math.ceil(bounds.size.y) + _TEXT_RENDER_GUTTER * 2)
end

function WindowMessageUI:updateLayoutByTextSize()
    local nameBounds = self._nameText:getLocalBounds()
    local hasName = self._nameText:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = getTextRenderWidth(TextLayout.measurePlainText(self._nameText, self._name))
        nameHeight = WindowMessageLayout.GetTextLineHeight(nameBounds)
    end
    local maxContentWidth = math.trunc(math.max(32, getMaxWindowWidth() - _WINDOW_PADDING * 2))
    self:setText("MessageText", self._message)
    local textBounds = self._messageText:getLocalBounds()
    local textWidth = getTextRenderWidth(TextLayout.measureRichText(_MESSAGE_TEXT_CONFIG, self._message))
    if textWidth > maxContentWidth then
        local displayMessage = WindowMessageLayout.WrapMessage(
            self._message, math.max(1.0, maxContentWidth - _TEXT_RENDER_GUTTER * 2.0), _MESSAGE_TEXT_CONFIG
        )
        self:setText("MessageText", displayMessage)
        textBounds = self._messageText:getLocalBounds()
        textWidth = getTextRenderWidth(TextLayout.measureRichText(_MESSAGE_TEXT_CONFIG, displayMessage))
    end
    local textHeight = getBoundsHeight(textBounds)
    local pauseMarkSize = self.model:getPauseMarkSize()
    ---@cast pauseMarkSize integer
    local contentWidth = math.trunc(math.max(textWidth, nameWidth, pauseMarkSize))
    contentWidth = math.trunc(math.min(contentWidth, maxContentWidth))
    local contentHeight = textHeight + pauseMarkSize
    if hasName then
        contentHeight = contentHeight + nameHeight + _NAME_MESSAGE_GAP
    end
    local totalWidth = contentWidth + _WINDOW_PADDING * 2
    totalWidth = math.min(totalWidth, getMaxWindowWidth())
    local totalHeight = contentHeight + _WINDOW_PADDING * 2
    self:_resizeWindow(totalWidth, totalHeight)
    WindowMessageLayout.ResizeCanvas(self._content, contentWidth, contentHeight)
    self._content:setPosition(sf.Vector2f.new(_WINDOW_PADDING, _WINDOW_PADDING))
    local textY = 0.0
    if hasName then
        self._nameText:setPosition(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        textY = nameHeight + _NAME_MESSAGE_GAP + 0.0
    end
    self._messageText:setPosition(
        sf.Vector2f.new(
            _TEXT_RENDER_GUTTER - textBounds.position.x, textY + _TEXT_RENDER_GUTTER - textBounds.position.y
        )
    )
    self.model:refreshPauseMarkLayout()
end

function WindowMessageUI:updateLayoutBySelectionSize()
    local nameBounds = self._nameText:getLocalBounds()
    local hasName = self._nameText:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = getTextRenderWidth(TextLayout.measurePlainText(self._nameText, self._name))
        nameHeight = WindowMessageLayout.GetTextLineHeight(nameBounds)
    end
    local maxOptionTextWidth = 1
    local optionCount = 0
    if self._selectionListView ~= nil then
        local children = self._selectionListView:getChildren()
        optionCount = #children
        for _, child in ipairs(children) do
            local optionWidth = 1.0
            if Class.isInstance(child, PlainText) or Class.isInstance(child, RichText) then
                ---@cast child Engine.PlainText | Engine.RichText
                optionWidth = child:getLocalBounds().size.x
            end
            maxOptionTextWidth = math.max(maxOptionTextWidth, math.max(1, math.round(optionWidth)))
        end
    end
    local contentWidth = math.trunc(math.max(32, nameWidth, maxOptionTextWidth + _SELECTION_LIST_HORIZONTAL_INSET))
    local maxContentWidth = math.trunc(math.max(32, getMaxWindowWidth() - _WINDOW_PADDING * 2))
    contentWidth = math.trunc(math.min(contentWidth, maxContentWidth))
    local contentHeight = optionCount * _OPTION_ITEM_HEIGHT
    if hasName then
        contentHeight = contentHeight + nameHeight + _NAME_MESSAGE_GAP
    end
    local totalWidth = contentWidth + _WINDOW_PADDING * 2
    totalWidth = math.min(totalWidth, getMaxWindowWidth())
    local totalHeight = contentHeight + _WINDOW_PADDING * 2
    self:_resizeWindow(totalWidth, totalHeight)
    WindowMessageLayout.ResizeCanvas(self._content, contentWidth, contentHeight)
    self._content:setPosition(sf.Vector2f.new(_WINDOW_PADDING, _WINDOW_PADDING))
    local currentY = 0.0
    if hasName then
        self._nameText:setPosition(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        currentY = nameHeight + _NAME_MESSAGE_GAP + 0.0
    end
    if self._selectionListView ~= nil then
        local listSize = sf.Vector2i.new(contentWidth, optionCount * _OPTION_ITEM_HEIGHT)
        ---@cast listSize sf.Vector2i
        self._selectionListView:setSize(listSize)
        self._selectionListView:setOrigin(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        self._selectionListView:setPosition(sf.Vector2f.new(contentWidth / 2.0, currentY))
    end
    self.model:refreshPauseMarkLayout()
end

function WindowMessageUI:_resizeWindow(width, height)
    self._panelSize = sf.Vector2f.new(width, height)
    self:reflow(width, height)
end

function WindowMessageUI:resetTextColour()
    self._nameText:setColour(sf.Color.White)
    self._messageText:setColour(sf.Color.White)
end

function WindowMessageUI:setMessageVisible(visible)
    self._messageText:setVisible(visible)
end

function WindowMessageUI:confirmMessageAdvancer()
    if self._messageAdvancer ~= nil then
        self._messageAdvancer:onConfirm(Engine.UiInputEventArguments.new({}))
    end
end

function WindowMessageUI:cancelSelection(index)
    local children = self._selectionListView:getChildren()
    if index == nil or index < 0 or index >= #children then
        return
    end
    local child = children[index + 1]
    if Class.isInstance(child, Engine.FunctionalBase) then
        ---@cast child Engine.ControlBase & Engine.FunctionalBase
        child:onCancel(Engine.UiInputEventArguments.new({}))
    end
end

function WindowMessageUI:refreshSelection(options)
    local children = self._selectionListView:getChildren()
    local optionCount = math.min(#options, 4)
    assert(#children == optionCount, "Selection option count changed during dialogue")
    for index = 1, optionCount do
        local child = children[index]
        ---@cast child Engine.PlainText
        child:setString(WindowMessageLayout.NormaliseText(assert(options[index])))
    end
end

function WindowMessageUI:getSelectionPosition(index, rowHeight)
    local columns = self._selectionListView:getColumns()
    if columns <= 0 then
        return nil
    end
    local position = self._selectionListView:getPosition()
    local origin = self._selectionListView:getOrigin()
    local columnWidth = self._selectionListView:getSize().x / columns
    return sf.Vector2f.new(
        position.x - origin.x + index % columns * columnWidth,
        position.y - origin.y + math.floor(index / columns) * rowHeight
    )
end

function WindowMessageUI:getSelectionWidth()
    local columns = self._selectionListView:getColumns()
    if columns <= 0 then
        return nil
    end
    return math.max(1, math.round(self._selectionListView:getSize().x / columns))
end

return Ui.Define("WindowMessage", WindowMessageUI)
