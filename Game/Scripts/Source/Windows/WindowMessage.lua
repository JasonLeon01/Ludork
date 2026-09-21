local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local System = require("Source.System")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowMessageLayout = require("Source.Windows.WindowMessage.Layout")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local MessageOptionRowController = require("Source.Windows.WindowMessage.MessageOptionRow.Controller")
local Ui = require("Source.UIBase.Ui")
local UiLayout = require("Source.UIBase.UiLayout")
local View = require("Source.UI.WindowMessage")

local Display = GlobalCore.Display
local PlainText = Engine.PlainText
local RichText = Engine.RichText
local TextLayout = Engine.TextLayout
local _MESSAGE_TEXT_CONFIG = "UI/Message"

local _OPTION_ITEM_HEIGHT = 32

local _WINDOW_PADDING = 16

local _SCREEN_EDGE_MARGIN = 64

local _NAME_MESSAGE_GAP = 8

local _SELECTION_LIST_HORIZONTAL_INSET = 32

local _TEXT_RENDER_GUTTER = 2

local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager

local ContentMode = { MESSAGE = 0, SELECTION = 1 }

---@class Source.Windows.WindowMessage.Controller
local Controller = {}

Controller.windowOptions = {
    screen = true,
    hidden = true,
    transitionTarget = "Panel",
    itemWidth = 480,
    itemHeight = _OPTION_ITEM_HEIGHT
}

Controller._MAX_OPTIONS = 4
Controller.ContentMode = ContentMode

function Controller:init()
    self._inDialogue = false
    self._contentMode = ContentMode.MESSAGE
    self._selectionResult = nil
    self._allowCancel = true
    self._onFinished = nil
    self._pendingLayout = false
    self._pendingFadeIn = false
    self._pendingRefPosition = nil

    self._name = ""
    self._message = ""
    self._panelSize = sf.Vector2f.new(544.0, 160.0)
    self._messageAdvancer = nil
    self._messageRows = self:createCollection(self.ui.controls["MessageList"], MessageOptionRowController)
    self._selectionRows = self:createCollection(self.ui.controls["SelectionList"], MessageOptionRowController)
    self:_setupMessageAdvancer()
end

function Controller:_setupMessageAdvancer()
    local controller = self._messageRows:add({
        text = "",
        onConfirm = function (_itemSelf, _kwargs)
            self:_resolveSelection(0)
        end
    })
    self._messageAdvancer = controller.ui.root
    self._messageAdvancer:setVisible(false)
end

function Controller:setListView(listView)
    WindowSelectable.setListView(self.host, listView, true)
end

function Controller:onTick(deltaTime)
    if self._pendingLayout then
        self._pendingLayout = false
        if self._contentMode == ContentMode.SELECTION then
            self:updateLayoutBySelectionSize()
        else
            self:updateLayoutByTextSize()
        end
        self:updateWindowPosition(self._pendingRefPosition)
    end
    if self._pendingFadeIn then
        self._pendingFadeIn = false
        self.host:showWithAnimation("FadeIn", function ()
            self.host:setActive(true)
            self.host:requestKeyboardFocus()
            self:_onFadeInComplete()
        end)
    end
    if self._contentMode ~= ContentMode.SELECTION then
        self.host:hideSelectionCursor()
        self.host:detachSelectionRect()
        return WindowBase.onTick(self.host, deltaTime)
    end
    WindowSelectable.onTick(self.host, deltaTime)
end

function Controller:onKeyDown(kwargs)
    if self._contentMode == ContentMode.SELECTION and self._allowCancel
        and Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    return WindowSelectable.onKeyDown(self.host, kwargs)
end

function Controller:onReturn()
    if self._contentMode == ContentMode.SELECTION and self._allowCancel then
        self:cancelSelection(self.host.index)
    end
end

function Controller:_shouldCaptureTouch(position)
    if self._contentMode ~= ContentMode.SELECTION then
        return false
    end
    return self.host:shouldCaptureTouch(position)
end

function Controller:isInDialogue()
    return self._inDialogue
end

function Controller:isAwaitingMessageConfirm()
    return self._inDialogue and self._contentMode == ContentMode.MESSAGE and self._selectionResult == nil
end

function Controller:confirmMessage()
    if not self:isAwaitingMessageConfirm() then
        return false
    end
    if self._messageAdvancer ~= nil then
        self._messageAdvancer:onConfirm(Engine.UiInputEventArguments.new({}))
    end
    return true
end

function Controller:getSelectionResult()
    return self._selectionResult
end

---@param refPosition sf.Vector2f | nil
---@param name        string
---@param allowCancel boolean
---@param onFinished  fun() | nil
function Controller:_beginDialogue(refPosition, name, allowCancel, onFinished)
    self.host:hidePauseMark()
    self.host:setColour(sf.Color.White)
    self._inDialogue = true
    self._selectionResult = nil
    self._allowCancel = allowCancel
    self._onFinished = onFinished
    self:_setSpeakerName(WindowMessageLayout.NormaliseText(name))
    self:resetTextColour()
    self._pendingRefPosition = refPosition
end

function Controller:_finishDialogueSetup()
    self._pendingLayout = true
    self.host:setVisible(true)
    self.host:setActive(false)
    self._pendingFadeIn = true
end

---@param name string
function Controller:_refreshName(name)
    assert(self._inDialogue, "Message content can only be refreshed during dialogue")
    self:_setSpeakerName(WindowMessageLayout.NormaliseText(name))
end

function Controller:setMessage(refPosition, name, message, onFinished)
    self:_beginDialogue(refPosition, name, true, onFinished)
    self._contentMode = ContentMode.MESSAGE
    self:showMessageList()
    self.host.index = 0
    self:setMessageVisible(true)
    self:resetTextColour()
    self:_setMessageText(WindowMessageLayout.NormaliseText(message))
    self:setConfirmLayerActive(true)
    self:_finishDialogueSetup()
end

function Controller:setSelection(refPosition, name, options, allowCancel, onFinished)
    if allowCancel == nil then
        allowCancel = true
    end
    self:_beginDialogue(refPosition, name, allowCancel, onFinished)
    self._contentMode = ContentMode.SELECTION
    self:setConfirmLayerActive(false)
    self:_setMessageText("")
    self:setMessageVisible(false)
    local normalizedOptions = {}
    for _, option in ipairs(options) do
        normalizedOptions[#normalizedOptions + 1] = WindowMessageLayout.NormaliseText(option)
    end
    self:_setupSelectionList(normalizedOptions)
    self:_finishDialogueSetup()
end

function Controller:refreshMessage(name, message)
    assert(self._contentMode == ContentMode.MESSAGE, "Dialogue is not showing a message")
    self:_refreshName(name)
    self:_setMessageText(WindowMessageLayout.NormaliseText(message))
    self._pendingLayout = true
end

function Controller:refreshSelection(name, options)
    assert(self._contentMode == ContentMode.SELECTION, "Dialogue is not showing a selection")
    self:_refreshName(name)
    self:_refreshSelectionText(options)
    self._pendingLayout = true
end

---@param selectionResult integer
function Controller:_resolveSelection(selectionResult)
    if self._selectionResult ~= nil then
        return
    end
    self.host:hidePauseMark()
    self:setConfirmLayerActive(false)
    self._selectionResult = selectionResult
    self.host:setActive(false)
    self.host:hideWithAnimation("FadeOut", function ()
        self._inDialogue = false
        if self._onFinished ~= nil then
            local callbacks = { self._onFinished }
            self._onFinished = nil
            callbacks[1]()
        end
    end)
end

---@param options string[]
function Controller:_setupSelectionList(options)
    local limitedOptions = {}
    for index = 1, math.min(#options, self._MAX_OPTIONS) do
        limitedOptions[index] = assert(options[index])
    end
    self:showSelectionList(
        limitedOptions,
        function (optionIndex)
            AudioManager.playSound(System.GetDecisionSE())
            self:_resolveSelection(optionIndex)
        end,
        function ()
            AudioManager.playSound(System.GetCancelSE())
            self:_resolveSelection(-1)
        end
    )
    self.host.index = bool(limitedOptions) and 0 or nil
end

function Controller:_onFadeInComplete()
    if self._contentMode == ContentMode.MESSAGE then
        self.host:refreshPauseMarkLayout()
        self.host:showPauseMark()
    end
end

---@param index integer
---@return sf.Vector2f
function Controller:_getRectPositionForIndex(index)
    if self._contentMode == ContentMode.SELECTION then
        local position = self:getSelectionPosition(index, self.host:getSelectionRowHeight())
        if position ~= nil then
            return position
        end
    end
    return self.host:getSelectionPositionForIndex(index)
end

function Controller:getItemWidth()
    if self._contentMode == ContentMode.SELECTION then
        local width = self:getSelectionWidth()
        if width ~= nil then
            return width
        end
    end
    return WindowSelectable.getItemWidth(self.host)
end

function Controller:bind()
    self.ui.controls["SelectionList"]:setVisible(false)
    self.ui.controls["ConfirmLayer"]:addMouseButtonDownCallback(function (_layer, kwargs)
        if kwargs.button ~= sf.Mouse.Button.Left or not self:isAwaitingMessageConfirm() then
            return false
        end
        return self:confirmMessage()
    end)
    self.ui.controls["ConfirmLayer"]:addConfirmCallback(function ()
        self:confirmMessage()
    end)
    self:setConfirmLayerActive(false)
    self.ui.controls["WindowFrame"]:setColour(sf.Color.new(255, 255, 255, 192))
    self.ui.controls["NameText"]:setVisible(false)
end

function Controller:refresh()
    self:_setSpeakerName(self._name)
    self:_setMessageText(self._message)
end

function Controller:prepare()
    local size = self.host:getSize()
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    return super(Controller, self).prepare(logicalSize)
end

---@param active boolean
function Controller:setConfirmLayerActive(active)
    self.ui.controls["ConfirmLayer"]:setActive(active)
    self.ui.controls["ConfirmLayer"]:setVisible(active)
end

function Controller:_setSpeakerName(text)
    self._name = text
    self.ui.controls["NameText"]:setVisible(text:match("%S") ~= nil)
    self:setText("NameText", text)
end

function Controller:_setMessageText(text)
    self._message = text
    self:setText("MessageText", text)
end

function Controller:showMessageList()
    self.ui.controls["SelectionList"]:setVisible(false)
    self:setListView(self.ui.controls["MessageList"])
end

function Controller:showSelectionList(options, onConfirm, onCancel)
    self._selectionRows:clear()
    for luaIndex, optionText in ipairs(options) do
        local optionIndex = luaIndex - 1
        local controller = self._selectionRows:add({
            text = optionText,
            onConfirm = function (_itemSelf, _kwargs)
                onConfirm(optionIndex)
            end,
            onCancel = function (_itemSelf, _kwargs)
                onCancel()
            end
        })
        controller.ui.root:setColour(sf.Color.White)
        self.host:applyItem(controller.ui.root)
    end
    self._selectionRows:layout()
    self.ui.controls["SelectionList"]:setVisible(true)
    self:setListView(self.ui.controls["SelectionList"])
    return self.ui.controls["SelectionList"]
end

function Controller:updateWindowPosition(refPosition)
    local gameSize = Display.getGameSize()
    local gameWidth = gameSize.x + 0.0
    local gameHeight = gameSize.y + 0.0
    if refPosition == nil then
        local posX = (gameWidth - self._panelSize.x) / 2.0
        local posY = (gameHeight - self._panelSize.y) / 2.0
        self.ui.controls["Panel"]:setPosition(sf.Vector2f.new(posX, posY))
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
        self.ui.controls["Panel"]:setPosition(sf.Vector2f.new(posX, posY))
    end
end

local function getMaxWindowWidth()
    local gameWidth = Display.getGameSize().x
    return math.max(1, gameWidth - _SCREEN_EDGE_MARGIN)
end

local function getTextRenderWidth(measuredWidth)
    return math.max(1, math.ceil(measuredWidth) + _TEXT_RENDER_GUTTER * 2)
end

local function getBoundsHeight(bounds)
    return math.max(1, math.ceil(bounds.size.y) + _TEXT_RENDER_GUTTER * 2)
end

function Controller:updateLayoutByTextSize()
    local nameBounds = self.ui.controls["NameText"]:getLocalBounds()
    local hasName = self.ui.controls["NameText"]:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = getTextRenderWidth(TextLayout.measurePlainText(self.ui.controls["NameText"], self._name))
        nameHeight = WindowMessageLayout.GetTextLineHeight(nameBounds)
    end
    local maxContentWidth = math.trunc(math.max(32, getMaxWindowWidth() - _WINDOW_PADDING * 2))
    self:setText("MessageText", self._message)
    local textBounds = self.ui.controls["MessageText"]:getLocalBounds()
    local textWidth = getTextRenderWidth(TextLayout.measureRichText(_MESSAGE_TEXT_CONFIG, self._message))
    if textWidth > maxContentWidth then
        local displayMessage = WindowMessageLayout.WrapMessage(
            self._message, math.max(1.0, maxContentWidth - _TEXT_RENDER_GUTTER * 2.0), _MESSAGE_TEXT_CONFIG
        )
        self:setText("MessageText", displayMessage)
        textBounds = self.ui.controls["MessageText"]:getLocalBounds()
        textWidth = getTextRenderWidth(TextLayout.measureRichText(_MESSAGE_TEXT_CONFIG, displayMessage))
    end
    local textHeight = getBoundsHeight(textBounds)
    local pauseMarkSize = self.host:getPauseMarkSize()
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
    UiLayout.ResizeCanvas(self.ui.controls["Content"], contentWidth, contentHeight)
    self.ui.controls["Content"]:setPosition(sf.Vector2f.new(_WINDOW_PADDING, _WINDOW_PADDING))
    local textY = 0.0
    if hasName then
        self.ui.controls["NameText"]:setPosition(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        textY = nameHeight + _NAME_MESSAGE_GAP + 0.0
    end
    self.ui.controls["MessageText"]:setPosition(
        sf.Vector2f.new(
            _TEXT_RENDER_GUTTER - textBounds.position.x, textY + _TEXT_RENDER_GUTTER - textBounds.position.y
        )
    )
    self.host:refreshPauseMarkLayout()
end

function Controller:updateLayoutBySelectionSize()
    local nameBounds = self.ui.controls["NameText"]:getLocalBounds()
    local hasName = self.ui.controls["NameText"]:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = getTextRenderWidth(TextLayout.measurePlainText(self.ui.controls["NameText"], self._name))
        nameHeight = WindowMessageLayout.GetTextLineHeight(nameBounds)
    end
    local maxOptionTextWidth = 1
    local optionCount = 0
    if self.ui.controls["SelectionList"] ~= nil then
        local children = self.ui.controls["SelectionList"]:getChildren()
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
    UiLayout.ResizeCanvas(self.ui.controls["Content"], contentWidth, contentHeight)
    self.ui.controls["Content"]:setPosition(sf.Vector2f.new(_WINDOW_PADDING, _WINDOW_PADDING))
    local currentY = 0.0
    if hasName then
        self.ui.controls["NameText"]:setPosition(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        currentY = nameHeight + _NAME_MESSAGE_GAP + 0.0
    end
    if self.ui.controls["SelectionList"] ~= nil then
        local listSize = sf.Vector2i.new(contentWidth, optionCount * _OPTION_ITEM_HEIGHT)
        ---@cast listSize sf.Vector2i
        self.ui.controls["SelectionList"]:setSize(listSize)
        self.ui.controls["SelectionList"]:setOrigin(sf.Vector2f.new(contentWidth / 2.0, 0.0))
        self.ui.controls["SelectionList"]:setPosition(sf.Vector2f.new(contentWidth / 2.0, currentY))
    end
    self.host:refreshPauseMarkLayout()
end

function Controller:_resizeWindow(width, height)
    self._panelSize = sf.Vector2f.new(width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    UiLayout.ResizeCanvas(self.ui.controls["Panel"], width, height)
    self.ui.controls["WindowFrame"]:resize(logicalSize)
end

function Controller:resetTextColour()
    self.ui.controls["NameText"]:setColour(sf.Color.White)
    self.ui.controls["MessageText"]:setColour(sf.Color.White)
end

function Controller:setMessageVisible(visible)
    self.ui.controls["MessageText"]:setVisible(visible)
end

function Controller:cancelSelection(index)
    local children = self.ui.controls["SelectionList"]:getChildren()
    if index == nil or index < 0 or index >= #children then
        return
    end
    local child = children[index + 1]
    if Class.isInstance(child, Engine.FunctionalBase) then
        ---@cast child Engine.ControlBase & Engine.FunctionalBase
        child:onCancel(Engine.UiInputEventArguments.new({}))
    end
end

function Controller:_refreshSelectionText(options)
    local children = self.ui.controls["SelectionList"]:getChildren()
    local optionCount = math.min(#options, 4)
    assert(#children == optionCount, "Selection option count changed during dialogue")
    for index = 1, optionCount do
        local child = children[index]
        ---@cast child Engine.PlainText
        child:setString(WindowMessageLayout.NormaliseText(assert(options[index])))
    end
end

function Controller:getSelectionPosition(index, rowHeight)
    local columns = self.ui.controls["SelectionList"]:getColumns()
    if columns <= 0 then
        return nil
    end
    local position = self.ui.controls["SelectionList"]:getPosition()
    local origin = self.ui.controls["SelectionList"]:getOrigin()
    local columnWidth = self.ui.controls["SelectionList"]:getSize().x / columns
    return sf.Vector2f.new(
        position.x - origin.x + index % columns * columnWidth,
        position.y - origin.y + math.floor(index / columns) * rowHeight
    )
end

function Controller:getSelectionWidth()
    local columns = self.ui.controls["SelectionList"]:getColumns()
    if columns <= 0 then
        return nil
    end
    return math.max(1, math.round(self.ui.controls["SelectionList"]:getSize().x / columns))
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
