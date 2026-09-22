local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local System = require("Source.System")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowMessageLayout = require("Source.Windows.WindowMessage.Layout")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local MessageOptionRowController = require("Source.Windows.WindowMessage.MessageOptionRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowMessage")

local Display = GlobalCore.Display
local TextLayout = Engine.TextLayout
local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager

local ContentMode = { MESSAGE = 0, SELECTION = 1 }

---@class Source.Windows.WindowMessage.Controller
local Controller = {}

Controller.windowOptions = { screen = true, hidden = true, transitionTarget = "Panel" }

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
    local panelSize = self.ui.controls["Panel"]:getSize()
    local contentSize = self.ui.controls["Content"]:getSize()
    self._panelSize = sf.Vector2f.new(panelSize.x, panelSize.y)
    self._contentInsets = sf.Vector2f.new(panelSize.x - contentSize.x, panelSize.y - contentSize.y)
    self._screenInsets = self.root:getSize().x - self.ui.controls["DialogBounds"]:getSize().x
    self._nameGap = self.ui.controls["MessageBody"]:getPosition().y - self.ui.controls["NameArea"]:getSize().y
    self._bodyPosition = self.ui.controls["MessageBody"]:getPosition()
    self._selectionPosition = self.ui.controls["SelectionList"]:getPosition()
    self._textPadding = self.ui.controls["MessageText"]:getPosition()
        + self.ui.controls["MessageText"]:getLocalBounds().position
    self._nameColour = self.ui.controls["NameText"]:getColour()
    self._messageColour = self.ui.controls["MessageText"]:getColour()
    self._hostColour = self.host:getColour()
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
    WindowSelectable.setListView(self.host, listView)
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
    self.host:setColour(self._hostColour)
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
        self.host:showPauseMark()
    end
end

function Controller:getSelectionLayoutRect(index)
    local list = assert(self.host:getListView())
    local bounds = list:getItemLayoutRect(index)
    bounds.position.x = 0.0
    bounds.size.x = list:getSize().x
    return bounds
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
        self._selectionRows:add({
            text = optionText,
            onConfirm = function (_itemSelf, _kwargs)
                onConfirm(optionIndex)
            end,
            onCancel = function (_itemSelf, _kwargs)
                onCancel()
            end
        })
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

function Controller:_getMaxContentWidth()
    return math.max(1, Display.getGameSize().x - self._screenInsets - self._contentInsets.x)
end

function Controller:_getNameSize()
    if not self.ui.controls["NameText"]:getVisible() then
        return sf.Vector2f.new(0, 0)
    end
    local width = TextLayout.measurePlainText(self.ui.controls["NameText"], self._name)
    local height = WindowMessageLayout.GetTextLineHeight(self.ui.controls["NameText"]:getLocalBounds())
    return sf.Vector2f.new(math.ceil(width) + self._textPadding.x * 2, height)
end

function Controller:_layoutContent(contentWidth, bodyHeight, nameSize)
    local headerHeight = nameSize.y > 0 and nameSize.y + self._nameGap or 0
    local contentHeight = bodyHeight + headerHeight
    local width = math.ceil(contentWidth + self._contentInsets.x)
    local height = math.ceil(contentHeight + self._contentInsets.y)
    local panelSize = sf.Vector2u.new(width, height)
    ---@cast panelSize sf.Vector2u
    self.view:reflowControl("Panel", panelSize)
    self._panelSize = sf.Vector2f.new(width, height)
    local nameAreaSize = sf.Vector2u.new(math.ceil(contentWidth), math.max(1, math.ceil(nameSize.y)))
    ---@cast nameAreaSize sf.Vector2u
    self.view:reflowControl("NameArea", nameAreaSize)
    local bodySize = sf.Vector2u.new(math.ceil(contentWidth), math.max(1, math.ceil(bodyHeight)))
    ---@cast bodySize sf.Vector2u
    self.view:reflowControl("MessageBody", bodySize)
    self.ui.controls["MessageBody"]:setPosition(sf.Vector2f.new(self._bodyPosition.x, headerHeight))
    self.view:reflowControl("SelectionList", bodySize)
    self.ui.controls["SelectionList"]:setPosition(sf.Vector2f.new(self._selectionPosition.x, headerHeight))
end

function Controller:updateLayoutByTextSize()
    local nameSize = self:_getNameSize()
    local maxContentWidth = self:_getMaxContentWidth()
    self:setText("MessageText", self._message)
    local textWidth = math.ceil(TextLayout.measureRichText(self.ui.controls["MessageText"], self._message))
        + self._textPadding.x * 2
    if textWidth > maxContentWidth then
        local displayMessage = WindowMessageLayout.WrapMessage(
            self._message, math.max(1, maxContentWidth - self._textPadding.x * 2), self.ui.controls["MessageText"]
        )
        self:setText("MessageText", displayMessage)
        textWidth = math.ceil(TextLayout.measureRichText(self.ui.controls["MessageText"], displayMessage))
            + self._textPadding.x * 2
    end
    local textHeight = math.max(
        1, math.ceil(self.ui.controls["MessageText"]:getLocalBounds().size.y) + self._textPadding.y * 2
    )
    local pauseMarkSize = self.host:getPauseMarkSize()
    local contentWidth = math.min(maxContentWidth, math.max(textWidth, nameSize.x, pauseMarkSize))
    self:_layoutContent(contentWidth, textHeight + pauseMarkSize, nameSize)
end

function Controller:updateLayoutBySelectionSize()
    local nameSize = self:_getNameSize()
    local maxOptionTextWidth = 1
    local children = self.ui.controls["SelectionList"]:getChildren()
    for _, child in ipairs(children) do
        maxOptionTextWidth = math.max(maxOptionTextWidth, math.ceil(child:getLocalBounds().size.x))
    end
    local defaultSize = self.ui.controls["SelectionList"]:getDefaultItemSize()
    local inset = self.ui.controls["SelectionList"]:getSize().x
        - defaultSize.x * self.ui.controls["SelectionList"]:getColumns()
    local contentWidth = math.min(self:_getMaxContentWidth(), math.max(nameSize.x, maxOptionTextWidth + inset))
    local bodyHeight = #children * defaultSize.y
    self:_layoutContent(contentWidth, bodyHeight, nameSize)
end

function Controller:resetTextColour()
    self.ui.controls["NameText"]:setColour(self._nameColour)
    self.ui.controls["MessageText"]:setColour(self._messageColour)
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

return Ui.DefineWindow(View, Controller, WindowSelectable)
