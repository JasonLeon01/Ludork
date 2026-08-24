local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local System = require("Source.System")
local Data = require("Source.Data")
local TextLayout = require("Source.TextLayout")
local WindowMessageUI = require("Source.UI.WindowMessage")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local PlainText = Engine.PlainText
local RichText = Engine.RichText
local FunctionalBase = Engine.FunctionalBase
local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

local FadePhase = { NOTHING = 0, IN = 1, OUT = 2 }

local ContentMode = { MESSAGE = 0, SELECTION = 1 }

local _FADE_IN_CURVE_KEY = "WindowMessageFadeIn"
local _FADE_OUT_CURVE_KEY = "WindowMessageFadeOut"
local _FALLBACK_FADE_SPEED = 1000.0
local _NAME_TEXT_CONFIG = "UI/CenterText24"
local _MESSAGE_TEXT_CONFIG = "UI/Message"
local _OPTION_TEXT_CONFIG = "UI/Default"

---@type function
local normalizeText
---@type function
local getFadeCurve
---@type function
local getCurveDuration
---@type function
local getTextLineHeight
---@type function
local wrapMessage
---@type function
local resizeCanvas
---@class Source.Windows.WindowMessage
local WindowMessage = {}

WindowMessage._WINDOW_PADDING = 16
WindowMessage._SCREEN_EDGE_MARGIN = 64
WindowMessage._NAME_MESSAGE_GAP = 8
WindowMessage._OPTION_ITEM_HEIGHT = 32
WindowMessage._SELECTION_LIST_HORIZONTAL_INSET = 32
WindowMessage._TEXT_RENDER_GUTTER = 2
WindowMessage._MAX_OPTIONS = 4
WindowMessage._fadeCurves = {}

function WindowMessage:init()
    self._inDialogue = false
    self._contentMode = ContentMode.MESSAGE
    self._selectionListView = nil
    self._messageListView = nil
    self._messageAdvancer = nil
    self._selectionResult = nil
    self._allowCancel = true
    self._onFinished = nil
    self._fadePhase = FadePhase.NOTHING
    self._fadeTime = 0.0
    self._pendingLayout = false
    self._pendingRefPosition = nil
    self._name = ""
    self._message = ""

    super
        (WindowMessage, self)
        .init(Engine.ToIntRect(48, 288, 544, 160), nil, 480, self._OPTION_ITEM_HEIGHT, nil, nil, nil, nil, true)
    self._ui = WindowMessageUI.new(self)
    self._ui:attach()
    self._nameText = self._ui:getNameText()
    self._text = self._ui:getMessageText()
    self:setColour(sf.Color.new(255, 255, 255, 0))
    self._window:setColour(sf.Color.new(255, 255, 255, 192))
    self._nameText:setVisible(false)
    self:_setupMessageAdvancer()
    self:setVisible(false)
end

function WindowMessage:_setupMessageAdvancer()
    self._messageAdvancer, self._messageListView = self._ui:attachMessageAdvancer(function (_itemSelf, _kwargs)
        self:_resolveSelection(0)
    end)
end

function WindowMessage:onTick(deltaTime)
    if self._fadePhase == FadePhase.IN then
        self:_fadeIn(deltaTime)
    end
    if self._fadePhase == FadePhase.OUT then
        self:_fadeOut(deltaTime)
    end
    if self._pendingLayout then
        self._pendingLayout = false
        if self._contentMode == ContentMode.SELECTION then
            self:_updateLayoutBySelectionSize()
        else
            self:_updateLayoutByTextSize()
        end
        self:_updateWindowPosition(self._pendingRefPosition)
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

function WindowMessage:onClick(kwargs)
    if not self:getVisible() then
        return super(WindowMessage, self).onClick(kwargs)
    end
    if not self:isAwaitingMessageConfirm() then
        return super(WindowMessage, self).onClick(kwargs)
    end
    self:confirmMessage()
    return super(WindowMessage, self).onClick(kwargs)
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
    return self._inDialogue and self._contentMode == ContentMode.MESSAGE
        and self._fadePhase ~= FadePhase.OUT and self._selectionResult == nil
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

---@param text string
---@return string
function normalizeText(text)
    return (text:gsub("\\n", "\n"))
end

function WindowMessage:setMessage(refPosition, name, message, allowCancel, onFinished)
    if allowCancel == nil then
        allowCancel = true
    end
    self:hidePauseMark()
    self:setColour(sf.Color.new(255, 255, 255, 0))
    self._inDialogue = true
    self._selectionResult = nil
    self._allowCancel = allowCancel
    self._onFinished = onFinished
    self._fadePhase = FadePhase.IN
    self._fadeTime = 0.0
    self._name = normalizeText(name)
    self._ui:setName(self._name)
    self._nameText:setVisible(self._name:match("%S") ~= nil)
    self._nameText:setColour(sf.Color.new(255, 255, 255, 0))
    if type(message) == "table" then
        self._contentMode = ContentMode.SELECTION
        self._message = ""
        self._ui:setMessage(self._message)
        self._text:setVisible(false)
        local options = {}
        for _, item in ipairs(message) do
            options[#options + 1] = normalizeText(tostring(item))
        end
        self:_setupSelectionList(options)
        if self._selectionListView ~= nil then
            for _, child in ipairs(self._selectionListView:getChildren()) do
                ---@cast child Engine.PlainText
                child:setColour(sf.Color.new(255, 255, 255, 0))
            end
        end
    else
        self._contentMode = ContentMode.MESSAGE
        self._message = normalizeText(message)
        self._ui:showMessageList()
        self.index = 0
        self._text:setVisible(true)
        self._text:setColour(sf.Color.new(255, 255, 255, 0))
        self._ui:setMessage(self._message)
    end
    self._pendingLayout = true
    self._pendingRefPosition = refPosition
    self:setVisible(true)
    self:requestKeyboardFocus()
end

function WindowMessage:refreshContent(name, message)
    assert(self._inDialogue, "Message content can only be refreshed during dialogue")
    self._name = normalizeText(name)
    self._ui:setName(self._name)
    self._nameText:setVisible(self._name:match("%S") ~= nil)
    if self._contentMode == ContentMode.SELECTION then
        assert(type(message) == "table", "Selection dialogue requires option text")
        local selectionListView = assert(self._selectionListView, "Selection list is missing")
        local children = selectionListView:getChildren()
        local optionCount = math.min(#message, self._MAX_OPTIONS)
        assert(#children == optionCount, "Selection option count changed during dialogue")
        for index = 1, optionCount do
            local child = children[index]
            ---@cast child Engine.PlainText
            child:setString(normalizeText(tostring(message[index])))
        end
    else
        assert(type(message) == "string", "Message dialogue requires text")
        self._message = normalizeText(message)
        self._ui:setMessage(self._message)
    end
    self._pendingLayout = true
end

---@param selectionResult integer
function WindowMessage:_resolveSelection(selectionResult)
    if self._selectionResult ~= nil then
        return
    end
    self:hidePauseMark()
    self._selectionResult = selectionResult
    self._fadePhase = FadePhase.OUT
    self._fadeTime = 0.0
    if self._onFinished ~= nil then
        local callbacks = { self._onFinished }
        self._onFinished = nil
        callbacks[1]()
    end
end

---@param options table
function WindowMessage:_setupSelectionList(options)
    local limitedOptions = {}
    for index = 1, math.min(#options, self._MAX_OPTIONS) do
        limitedOptions[index] = options[index]
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
    local windowSize = self:getSize()
    local windowWidth = windowSize.x + 0.0
    local windowHeight = windowSize.y + 0.0
    if refPosition == nil then
        local posX = (gameWidth - windowWidth) / 2.0
        local posY = (gameHeight - windowHeight) / 2.0
        self:setPosition(sf.Vector2f.new(posX, posY))
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
        self:setPosition(sf.Vector2f.new(posX, posY))
    end
end

---@param deltaTime number
function WindowMessage:_fadeIn(deltaTime)
    ---@type any[]
    local fadeTargets = { self, self._nameText }
    if self._contentMode == ContentMode.MESSAGE then
        fadeTargets[#fadeTargets + 1] = self._text
    elseif self._contentMode == ContentMode.SELECTION and self._selectionListView ~= nil then
        for _, child in ipairs(self._selectionListView:getChildren()) do
            fadeTargets[#fadeTargets + 1] = child
        end
    end
    self._fadeTime = self._fadeTime + deltaTime
    local curve = getFadeCurve(_FADE_IN_CURVE_KEY)
    local alpha = nil
    local duration = nil
    if bool(curve) and bool(curve.keys) then
        alpha = math.floor(Engine.Clamp(curve:evaluate(self._fadeTime), 0.0, 255.0))
        duration = getCurveDuration(curve)
    else
        alpha = math.floor(math.min(255.0, self._fadeTime * _FALLBACK_FADE_SPEED))
        duration = 255.0 / _FALLBACK_FADE_SPEED
    end
    for _, component in ipairs(fadeTargets) do
        component:setColour(sf.Color.new(255, 255, 255, alpha))
    end
    if self._fadeTime >= duration then
        self._fadePhase = FadePhase.NOTHING
        self:_onFadeInComplete()
    end
end

function WindowMessage:_onFadeInComplete()
    if self._contentMode == ContentMode.MESSAGE then
        self:refreshPauseMarkLayout()
        self:showPauseMark()
    end
end

---@param deltaTime number
function WindowMessage:_fadeOut(deltaTime)
    self._fadeTime = self._fadeTime + deltaTime
    local curve = getFadeCurve(_FADE_OUT_CURVE_KEY)
    local alpha = nil
    local duration = nil
    if bool(curve) and bool(curve.keys) then
        alpha = math.floor(Engine.Clamp(curve:evaluate(self._fadeTime), 0.0, 255.0))
        duration = getCurveDuration(curve)
    else
        alpha = math.floor(math.max(0.0, 255.0 - self._fadeTime * _FALLBACK_FADE_SPEED))
        duration = 255.0 / _FALLBACK_FADE_SPEED
    end
    self:setColour(sf.Color.new(255, 255, 255, alpha))
    if self._fadeTime >= duration or alpha == 0 then
        self._fadePhase = FadePhase.NOTHING
        self._inDialogue = false
        self:setVisible(false)
    end
end

---@param key string
---@return Engine.Curve
function getFadeCurve(key)
    local cached = WindowMessage._fadeCurves[key]
    if cached ~= nil then
        return cached
    end
    local curve = Data.GetCurve(key)
    WindowMessage._fadeCurves[key] = curve
    return curve
end

---@param curve Engine.Curve | nil
---@return number
function getCurveDuration(curve)
    if curve == nil or #curve.keys < 2 then
        return 0.0
    end
    local firstKey = curve.keys[1]
    local lastKey = curve.keys[#curve.keys]
    return lastKey.time - firstKey.time
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

---@param bounds sf.FloatRect
---@return integer
function getTextLineHeight(bounds)
    return math.max(1, math.ceil(bounds.position.y + bounds.size.y))
end

---@param text     string
---@param maxWidth number
---@return string
function wrapMessage(text, maxWidth)
    return TextLayout.WrapRichText(text, maxWidth, _MESSAGE_TEXT_CONFIG)
end

function WindowMessage:_updateLayoutByTextSize()
    local nameBounds = self._nameText:getLocalBounds()
    local hasName = self._nameText:getVisible()
    local nameWidth = 0
    local nameHeight = 0
    if hasName then
        nameWidth = self:_getTextRenderWidth(TextLayout.MeasurePlainText(_NAME_TEXT_CONFIG, self._name))
        nameHeight = getTextLineHeight(nameBounds)
    end
    ---@cast nameWidth integer
    local displayMessage = self._message .. ""
    local maxContentWidth = Engine.ToInteger(math.max(32, self:_getMaxWindowWidth() - self._WINDOW_PADDING * 2))
    self._ui:setMessage(displayMessage)
    local textBounds = self._text:getLocalBounds()
    local textWidth = self:_getTextRenderWidth(TextLayout.MeasureRichText(_MESSAGE_TEXT_CONFIG, displayMessage))
    if textWidth > maxContentWidth then
        displayMessage = wrapMessage(displayMessage, math.max(1.0, maxContentWidth - self._TEXT_RENDER_GUTTER * 2.0))
        self._ui:setMessage(displayMessage)
        textBounds = self._text:getLocalBounds()
        textWidth = self:_getTextRenderWidth(TextLayout.MeasureRichText(_MESSAGE_TEXT_CONFIG, displayMessage))
    end
    ---@cast textWidth integer
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
    resizeCanvas(self, totalWidth, totalHeight)
    self:_resizeWindow(totalWidth, totalHeight)
    resizeCanvas(self.content, contentWidth, contentHeight)
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
        nameWidth = self:_getTextRenderWidth(TextLayout.MeasurePlainText(_NAME_TEXT_CONFIG, self._name))
        nameHeight = getTextLineHeight(nameBounds)
    end
    ---@cast nameWidth integer
    local maxOptionTextWidth = 1
    local optionCount = 0
    if self._selectionListView ~= nil then
        local children = self._selectionListView:getChildren()
        optionCount = #children
        for _, child in ipairs(children) do
            local optionText = ""
            if Class.isInstance(child, PlainText) or Class.isInstance(child, RichText) then
                ---@cast child any
                optionText = child:getString()
            end
            maxOptionTextWidth = math.max(
                maxOptionTextWidth,
                math.max(1, Engine.Round(TextLayout.MeasurePlainText(_OPTION_TEXT_CONFIG, optionText)))
            )
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
    resizeCanvas(self, totalWidth, totalHeight)
    self:_resizeWindow(totalWidth, totalHeight)
    resizeCanvas(self.content, contentWidth, contentHeight)
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

---@param target Engine.Canvas
---@param width  integer
---@param height integer
function resizeCanvas(target, width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
    target:setView(target:getDefaultView())
end

---@param width  integer
---@param height integer
function WindowMessage:_resizeWindow(width, height)
    self._ui:reflow(width, height)
end

WindowMessage.FadePhase = FadePhase
WindowMessage.ContentMode = ContentMode

return class(WindowMessage, WindowSelectable)
