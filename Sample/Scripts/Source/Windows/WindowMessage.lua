local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local System = require("Source.System")
local WindowMessageUI = require("Source.UI.WindowMessage")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowMessageLayout = require("Source.Windows.WindowMessageLayout")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local PlainText = Engine.PlainText
local TextLayout = Engine.TextLayout
local RichText = Engine.RichText
local FunctionalBase = Engine.FunctionalBase
local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

local ContentMode = { MESSAGE = 0, SELECTION = 1 }

local _MESSAGE_TEXT_CONFIG = "UI/Message"

---@class Source.Windows.WindowMessage
local WindowMessage = {}

WindowMessage._WINDOW_PADDING = 16
WindowMessage._SCREEN_EDGE_MARGIN = 64
WindowMessage._NAME_MESSAGE_GAP = 8
WindowMessage._OPTION_ITEM_HEIGHT = 32
WindowMessage._SELECTION_LIST_HORIZONTAL_INSET = 32
WindowMessage._TEXT_RENDER_GUTTER = 2
WindowMessage._MAX_OPTIONS = 4
function WindowMessage:init()
    local gameSize = GlobalSystem.getGameSize()
    self._inDialogue = false
    self._contentMode = ContentMode.MESSAGE
    self._selectionListView = nil
    self._messageListView = nil
    self._messageAdvancer = nil
    self._selectionResult = nil
    self._allowCancel = true
    self._onFinished = nil
    self._pendingLayout = false
    self._pendingFadeIn = false
    self._pendingRefPosition = nil
    self._name = ""
    self._message = ""
    self._panelSize = sf.Vector2f.new(544.0, 160.0)

    super
        (WindowMessage, self)
        .init(Engine.ToIntRect(0, 0, gameSize.x, gameSize.y), nil, 480, self._OPTION_ITEM_HEIGHT, nil, nil, nil, nil, true)
    self._ui = WindowMessageUI.new(self)
    self._ui:attach()
    self._nameText = self._ui:getNameText()
    self._text = self._ui:getMessageText()
    self._window:setColour(sf.Color.new(255, 255, 255, 192))
    self._nameText:setVisible(false)
    self:_setupMessageAdvancer()
    self:hideImmediate()
end

function WindowMessage:_setupMessageAdvancer()
    self._messageAdvancer, self._messageListView = self._ui:attachMessageAdvancer(function (_itemSelf, _kwargs)
        self:_resolveSelection(0)
    end)
end

function WindowMessage:setListView(listView)
    if self._listView ~= nil and self._listView:getParent() == self.content then
        self.content:removeChild(self._listView)
    end
    if listView ~= nil and listView:getParent() ~= self.content then
        self.content:addChild(listView)
    end
    self._listView = listView
    self._ensureSelectionVisibleRequested = true
end

function WindowMessage:onTick(deltaTime)
    if self._pendingLayout then
        self._pendingLayout = false
        if self._contentMode == ContentMode.SELECTION then
            self:_updateLayoutBySelectionSize()
        else
            self:_updateLayoutByTextSize()
        end
        self:_updateWindowPosition(self._pendingRefPosition)
    end
    if self._pendingFadeIn then
        self._pendingFadeIn = false
        self:showWithAnimation("FadeIn", function ()
            self:setActive(true)
            self:requestKeyboardFocus()
            self:_onFadeInComplete()
        end)
    end
    if self._contentMode ~= ContentMode.SELECTION then
        self._rect:setVisible(false)
        self._ui:detachControl(self._rect)
        return WindowBase.onTick(self, deltaTime)
    end
    super(WindowMessage, self).onTick(deltaTime)
end

function WindowMessage:onKeyDown(kwargs)
    if self._contentMode == ContentMode.SELECTION and self._allowCancel
        and Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    return super(WindowMessage, self).onKeyDown(kwargs)
end

function WindowMessage:onReturn()
    if self._contentMode ~= ContentMode.SELECTION or not self._allowCancel
        or self._selectionListView == nil or self.index == nil then
        return
    end
    local children = self._selectionListView:getChildren()
    if self.index < 0 or self.index >= #children then
        return
    end
    local child = children[self.index + 1]
    if Class.isInstance(child, FunctionalBase) then
        ---@cast child Engine.ControlBase & Engine.FunctionalBase
        child:onCancel({})
    end
end

function WindowMessage:_shouldCaptureTouch(position)
    if self._contentMode ~= ContentMode.SELECTION then
        return false
    end
    return super(WindowMessage, self)._shouldCaptureTouch(position)
end

function WindowMessage:isInDialogue()
    return self._inDialogue
end

function WindowMessage:isAwaitingMessageConfirm()
    return self._inDialogue and self._contentMode == ContentMode.MESSAGE and self._selectionResult == nil
end

function WindowMessage:confirmMessage()
    if not self:isAwaitingMessageConfirm() then
        return false
    end
    if self._messageAdvancer ~= nil then
        self._messageAdvancer:onConfirm({})
    end
    return true
end

function WindowMessage:getSelectionResult()
    return self._selectionResult
end

---@param window      Source.Windows.WindowMessage
---@param refPosition sf.Vector2f | nil
---@param name        string
---@param allowCancel boolean
---@param onFinished  fun() | nil
local function beginDialogue(window, refPosition, name, allowCancel, onFinished)
    window:hidePauseMark()
    window:setColour(sf.Color.White)
    window._inDialogue = true
    window._selectionResult = nil
    window._allowCancel = allowCancel
    window._onFinished = onFinished
    window._name = WindowMessageLayout.NormaliseText(name)
    window._ui:setName(window._name)
    window._nameText:setVisible(window._name:match("%S") ~= nil)
    window._nameText:setColour(sf.Color.White)
    window._pendingRefPosition = refPosition
end

---@param window Source.Windows.WindowMessage
local function finishDialogueSetup(window)
    window._pendingLayout = true
    window:setVisible(true)
    window:setActive(false)
    window._pendingFadeIn = true
end

---@param window Source.Windows.WindowMessage
---@param name   string
local function refreshName(window, name)
    assert(window._inDialogue, "Message content can only be refreshed during dialogue")
    window._name = WindowMessageLayout.NormaliseText(name)
    window._ui:setName(window._name)
    window._nameText:setVisible(window._name:match("%S") ~= nil)
end

function WindowMessage:setMessage(refPosition, name, message, onFinished)
    beginDialogue(self, refPosition, name, true, onFinished)
    self._contentMode = ContentMode.MESSAGE
    self._message = WindowMessageLayout.NormaliseText(message)
    self._ui:showMessageList()
    self.index = 0
    self._text:setVisible(true)
    self._text:setColour(sf.Color.White)
    self._ui:setMessage(self._message)
    self._ui:setConfirmLayerActive(true)
    finishDialogueSetup(self)
end

function WindowMessage:setSelection(refPosition, name, options, allowCancel, onFinished)
    if allowCancel == nil then
        allowCancel = true
    end
    beginDialogue(self, refPosition, name, allowCancel, onFinished)
    self._contentMode = ContentMode.SELECTION
    self._ui:setConfirmLayerActive(false)
    self._message = ""
    self._ui:setMessage(self._message)
    self._text:setVisible(false)
    local normalizedOptions = {}
    for _, option in ipairs(options) do
        normalizedOptions[#normalizedOptions + 1] = WindowMessageLayout.NormaliseText(option)
    end
    self:_setupSelectionList(normalizedOptions)
    if self._selectionListView ~= nil then
        for _, child in ipairs(self._selectionListView:getChildren()) do
            ---@cast child Engine.PlainText
            child:setColour(sf.Color.White)
        end
    end
    finishDialogueSetup(self)
end

function WindowMessage:refreshMessage(name, message)
    assert(self._contentMode == ContentMode.MESSAGE, "Dialogue is not showing a message")
    refreshName(self, name)
    self._message = WindowMessageLayout.NormaliseText(message)
    self._ui:setMessage(self._message)
    self._pendingLayout = true
end

function WindowMessage:refreshSelection(name, options)
    assert(self._contentMode == ContentMode.SELECTION, "Dialogue is not showing a selection")
    refreshName(self, name)
    local selectionListView = assert(self._selectionListView, "Selection list is missing")
    local children = selectionListView:getChildren()
    local optionCount = math.min(#options, self._MAX_OPTIONS)
    assert(#children == optionCount, "Selection option count changed during dialogue")
    for index = 1, optionCount do
        local child = children[index]
        ---@cast child Engine.PlainText
        child:setString(WindowMessageLayout.NormaliseText(assert(options[index])))
    end
    self._pendingLayout = true
end

---@param selectionResult integer
function WindowMessage:_resolveSelection(selectionResult)
    if self._selectionResult ~= nil then
        return
    end
    self:hidePauseMark()
    self._ui:setConfirmLayerActive(false)
    self._selectionResult = selectionResult
    self:setActive(false)
    self:hideWithAnimation("FadeOut", function ()
        self._inDialogue = false
        if self._onFinished ~= nil then
            local callbacks = { self._onFinished }
            self._onFinished = nil
            callbacks[1]()
        end
    end)
end

---@param options string[]
function WindowMessage:_setupSelectionList(options)
    local limitedOptions = {}
    for index = 1, math.min(#options, self._MAX_OPTIONS) do
        limitedOptions[index] = assert(options[index])
    end
    self._selectionListView = self._ui:showSelectionList(
        limitedOptions,
        function (optionIndex)
            ManagerFunctions.playSE(System.GetDecisionSE())
            self:_resolveSelection(optionIndex)
        end,
        function ()
            ManagerFunctions.playSE(System.GetCancelSE())
            self:_resolveSelection(-1)
        end
    )
    self.index = bool(limitedOptions) and 0 or nil
end

---@param refPosition sf.Vector2f | nil
function WindowMessage:_updateWindowPosition(refPosition)
    local gameSize = GlobalSystem.getGameSize()
    local gameWidth = gameSize.x + 0.0
    local gameHeight = gameSize.y + 0.0
    local windowWidth = self._panelSize.x
    local windowHeight = self._panelSize.y
    if refPosition == nil then
        local posX = (gameWidth - windowWidth) / 2.0
        local posY = (gameHeight - windowHeight) / 2.0
        self._ui:setPanelPosition(sf.Vector2f.new(posX, posY))
    else
        local cellSize = Engine.CellSize + 0.0
        local anchorX = refPosition.x + cellSize * 0.5
        local halfScreenY = gameHeight * 0.5
        local posY = nil
        if refPosition.y < halfScreenY then
            posY = refPosition.y + cellSize
        else
            posY = refPosition.y - windowHeight
        end
        local posX = anchorX - windowWidth * 0.5
        local maxX = math.max(0.0, gameWidth - windowWidth)
        local maxY = math.max(0.0, gameHeight - windowHeight)
        posX = Engine.Clamp(posX, 0.0, maxX)
        posY = Engine.Clamp(posY, 0.0, maxY)
        self._ui:setPanelPosition(sf.Vector2f.new(posX, posY))
    end
end

function WindowMessage:_onFadeInComplete()
    if self._contentMode == ContentMode.MESSAGE then
        self:refreshPauseMarkLayout()
        self:showPauseMark()
    end
end

---@return integer
function WindowMessage:_getMaxWindowWidth()
    local gameWidth = GlobalSystem.getGameSize().x
    return math.max(1, gameWidth - self._SCREEN_EDGE_MARGIN)
end

---@param measuredWidth number
---@return integer
function WindowMessage:_getTextRenderWidth(measuredWidth)
    return math.max(1, math.ceil(measuredWidth) + self._TEXT_RENDER_GUTTER * 2)
end

---@param bounds sf.FloatRect
---@return integer
function WindowMessage:_getBoundsHeight(bounds)
    return math.max(1, math.ceil(bounds.size.y) + self._TEXT_RENDER_GUTTER * 2)
end

function WindowMessage:_updateLayoutByTextSize()
    local nameBounds = self._nameText:getLocalBounds()
    local hasName = self._nameText:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = self:_getTextRenderWidth(TextLayout.measurePlainText(self._nameText, self._name))
        nameHeight = WindowMessageLayout.GetTextLineHeight(nameBounds)
    end
    local displayMessage = self._message
    local maxContentWidth = Engine.ToInteger(math.max(32, self:_getMaxWindowWidth() - self._WINDOW_PADDING * 2))
    self._ui:setMessage(displayMessage)
    local textBounds = self._text:getLocalBounds()
    local textWidth = self:_getTextRenderWidth(TextLayout.measureRichText(_MESSAGE_TEXT_CONFIG, displayMessage))
    if textWidth > maxContentWidth then
        displayMessage = WindowMessageLayout.WrapMessage(
            displayMessage, math.max(1.0, maxContentWidth - self._TEXT_RENDER_GUTTER * 2.0), _MESSAGE_TEXT_CONFIG
        )
        self._ui:setMessage(displayMessage)
        textBounds = self._text:getLocalBounds()
        textWidth = self:_getTextRenderWidth(TextLayout.measureRichText(_MESSAGE_TEXT_CONFIG, displayMessage))
    end
    local textHeight = self:_getBoundsHeight(textBounds)
    local pauseMarkSize = WindowBase._PAUSE_MARK_SIZE
    ---@cast pauseMarkSize integer
    local contentWidth = Engine.ToInteger(math.max(textWidth, nameWidth, pauseMarkSize))
    contentWidth = Engine.ToInteger(math.min(contentWidth, maxContentWidth))
    local contentHeight = textHeight + pauseMarkSize
    if hasName then
        contentHeight = contentHeight + nameHeight + self._NAME_MESSAGE_GAP
    end
    local totalWidth = contentWidth + self._WINDOW_PADDING * 2
    totalWidth = math.min(totalWidth, self:_getMaxWindowWidth())
    local totalHeight = contentHeight + self._WINDOW_PADDING * 2
    self:_resizeWindow(totalWidth, totalHeight)
    WindowMessageLayout.ResizeCanvas(self.content, contentWidth, contentHeight)
    self.content:setPosition(sf.Vector2f.new(self._WINDOW_PADDING, self._WINDOW_PADDING))
    local textY = 0.0
    if hasName then
        self._nameText:setPosition(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        textY = nameHeight + self._NAME_MESSAGE_GAP + 0.0
    end
    self._text:setPosition(
        sf.Vector2f.new(
            self._TEXT_RENDER_GUTTER - textBounds.position.x, textY + self._TEXT_RENDER_GUTTER - textBounds.position.y
        )
    )
    self:refreshPauseMarkLayout()
end

function WindowMessage:_updateLayoutBySelectionSize()
    local nameBounds = self._nameText:getLocalBounds()
    local hasName = self._nameText:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = self:_getTextRenderWidth(TextLayout.measurePlainText(self._nameText, self._name))
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
            maxOptionTextWidth = math.max(maxOptionTextWidth, math.max(1, Engine.Round(optionWidth)))
        end
    end
    local contentWidth = Engine.ToInteger(
        math.max(32, nameWidth, maxOptionTextWidth + self._SELECTION_LIST_HORIZONTAL_INSET)
    )
    local maxContentWidth = Engine.ToInteger(math.max(32, self:_getMaxWindowWidth() - self._WINDOW_PADDING * 2))
    contentWidth = Engine.ToInteger(math.min(contentWidth, maxContentWidth))
    local contentHeight = optionCount * self._OPTION_ITEM_HEIGHT
    if hasName then
        contentHeight = contentHeight + nameHeight + self._NAME_MESSAGE_GAP
    end
    local totalWidth = contentWidth + self._WINDOW_PADDING * 2
    totalWidth = math.min(totalWidth, self:_getMaxWindowWidth())
    local totalHeight = contentHeight + self._WINDOW_PADDING * 2
    self:_resizeWindow(totalWidth, totalHeight)
    WindowMessageLayout.ResizeCanvas(self.content, contentWidth, contentHeight)
    self.content:setPosition(sf.Vector2f.new(self._WINDOW_PADDING, self._WINDOW_PADDING))
    local currentY = 0.0
    if hasName then
        self._nameText:setPosition(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        currentY = nameHeight + self._NAME_MESSAGE_GAP + 0.0
    end
    if self._selectionListView ~= nil then
        local listSize = sf.Vector2i.new(contentWidth, optionCount * self._OPTION_ITEM_HEIGHT)
        ---@cast listSize sf.Vector2i
        self._selectionListView:setSize(listSize)
        self._selectionListView:setOrigin(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        self._selectionListView:setPosition(sf.Vector2f.new(contentWidth / 2.0, currentY))
    end
    self:refreshPauseMarkLayout()
end

---@param index integer
---@return sf.Vector2f
function WindowMessage:_getRectPositionForIndex(index)
    if self._contentMode ~= ContentMode.SELECTION or self._selectionListView == nil then
        return super(WindowMessage, self)._getRectPositionForIndex(index)
    end
    local columns = self._selectionListView:getColumns()
    if columns <= 0 then
        return super(WindowMessage, self)._getRectPositionForIndex(index)
    end
    local listViewPosition = self._selectionListView:getPosition()
    local origin = self._selectionListView:getOrigin()
    local columnWidth = self._selectionListView:getSize().x / columns
    local column = index % columns
    local row = math.floor(index / columns)
    local x = listViewPosition.x - origin.x + column * columnWidth
    local y = listViewPosition.y - origin.y + row * self._rectHeight
    return sf.Vector2f.new(x, y)
end

---@return integer
function WindowMessage:_getRectWidth()
    if self._contentMode ~= ContentMode.SELECTION or self._selectionListView == nil then
        return super(WindowMessage, self)._getRectWidth()
    end
    local columns = self._selectionListView:getColumns()
    if columns <= 0 then
        return super(WindowMessage, self)._getRectWidth()
    end
    return math.max(1, Engine.Round(self._selectionListView:getSize().x / columns))
end

---@param width  integer
---@param height integer
function WindowMessage:_resizeWindow(width, height)
    self._panelSize = sf.Vector2f.new(width, height)
    self._ui:reflow(width, height)
end

WindowMessage.ContentMode = ContentMode

return class(WindowMessage, WindowSelectable)
