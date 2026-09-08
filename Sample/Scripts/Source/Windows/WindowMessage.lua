local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local System = require("Source.System")
local WindowMessageUI = require("Source.UI.WindowMessage")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowMessageLayout = require("Source.UI.WindowMessage.Layout")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager
local GlobalSystem = GlobalCore.System

local ContentMode = { MESSAGE = 0, SELECTION = 1 }

---@class Source.Windows.WindowMessage
local WindowMessage = {}

WindowMessage._OPTION_ITEM_HEIGHT = 32
WindowMessage._MAX_OPTIONS = 4
function WindowMessage:init()
    local gameSize = GlobalSystem.getGameSize()
    self._inDialogue = false
    self._contentMode = ContentMode.MESSAGE
    self._selectionResult = nil
    self._allowCancel = true
    self._onFinished = nil
    self._pendingLayout = false
    self._pendingFadeIn = false
    self._pendingRefPosition = nil

    super
        (WindowMessage, self)
        .init(Engine.ToIntRect(0, 0, gameSize.x, gameSize.y), nil, 480, self._OPTION_ITEM_HEIGHT, nil, nil, nil, nil, true)
    self._ui = WindowMessageUI.new(self)
    self._ui:attach()
    self:_setupMessageAdvancer()
    self:hideImmediate()
end

function WindowMessage:_setupMessageAdvancer()
    self._ui:attachMessageAdvancer(function (_itemSelf, _kwargs)
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
            self._ui:updateLayoutBySelectionSize()
        else
            self._ui:updateLayoutByTextSize()
        end
        self._ui:updateWindowPosition(self._pendingRefPosition)
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
    if self._contentMode == ContentMode.SELECTION and self._allowCancel then
        self._ui:cancelSelection(self.index)
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
    self._ui:confirmMessageAdvancer()
    return true
end

function WindowMessage:getSelectionResult()
    return self._selectionResult
end

---@param refPosition sf.Vector2f | nil
---@param name        string
---@param allowCancel boolean
---@param onFinished  fun() | nil
function WindowMessage:_beginDialogue(refPosition, name, allowCancel, onFinished)
    self:hidePauseMark()
    self:setColour(sf.Color.White)
    self._inDialogue = true
    self._selectionResult = nil
    self._allowCancel = allowCancel
    self._onFinished = onFinished
    self._ui:setName(WindowMessageLayout.NormaliseText(name))
    self._ui:resetTextColour()
    self._pendingRefPosition = refPosition
end

function WindowMessage:_finishDialogueSetup()
    self._pendingLayout = true
    self:setVisible(true)
    self:setActive(false)
    self._pendingFadeIn = true
end

---@param name string
function WindowMessage:_refreshName(name)
    assert(self._inDialogue, "Message content can only be refreshed during dialogue")
    self._ui:setName(WindowMessageLayout.NormaliseText(name))
end

function WindowMessage:setMessage(refPosition, name, message, onFinished)
    self:_beginDialogue(refPosition, name, true, onFinished)
    self._contentMode = ContentMode.MESSAGE
    self._ui:showMessageList()
    self.index = 0
    self._ui:setMessageVisible(true)
    self._ui:resetTextColour()
    self._ui:setMessage(WindowMessageLayout.NormaliseText(message))
    self._ui:setConfirmLayerActive(true)
    self:_finishDialogueSetup()
end

function WindowMessage:setSelection(refPosition, name, options, allowCancel, onFinished)
    if allowCancel == nil then
        allowCancel = true
    end
    self:_beginDialogue(refPosition, name, allowCancel, onFinished)
    self._contentMode = ContentMode.SELECTION
    self._ui:setConfirmLayerActive(false)
    self._ui:setMessage("")
    self._ui:setMessageVisible(false)
    local normalizedOptions = {}
    for _, option in ipairs(options) do
        normalizedOptions[#normalizedOptions + 1] = WindowMessageLayout.NormaliseText(option)
    end
    self:_setupSelectionList(normalizedOptions)
    self:_finishDialogueSetup()
end

function WindowMessage:refreshMessage(name, message)
    assert(self._contentMode == ContentMode.MESSAGE, "Dialogue is not showing a message")
    self:_refreshName(name)
    self._ui:setMessage(WindowMessageLayout.NormaliseText(message))
    self._pendingLayout = true
end

function WindowMessage:refreshSelection(name, options)
    assert(self._contentMode == ContentMode.SELECTION, "Dialogue is not showing a selection")
    self:_refreshName(name)
    self._ui:refreshSelection(options)
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
    self._ui:showSelectionList(
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
    self.index = bool(limitedOptions) and 0 or nil
end

function WindowMessage:_onFadeInComplete()
    if self._contentMode == ContentMode.MESSAGE then
        self:refreshPauseMarkLayout()
        self:showPauseMark()
    end
end

---@param index integer
---@return sf.Vector2f
function WindowMessage:_getRectPositionForIndex(index)
    if self._contentMode == ContentMode.SELECTION then
        local position = self._ui:getSelectionPosition(index, self._rectHeight)
        if position ~= nil then
            return position
        end
    end
    return super(WindowMessage, self)._getRectPositionForIndex(index)
end

function WindowMessage:getItemWidth()
    if self._contentMode == ContentMode.SELECTION then
        local width = self._ui:getSelectionWidth()
        if width ~= nil then
            return width
        end
    end
    return super(WindowMessage, self).getItemWidth()
end

WindowMessage.ContentMode = ContentMode

return class(WindowMessage, WindowSelectable)
