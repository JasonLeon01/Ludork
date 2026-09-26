local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local Canvas = Engine.Canvas
local TextureManager = GlobalCore.TextureManager

---@class Internal.UIBase.WindowBase
local WindowBase = {}

WindowBase._PAUSE_MARK_FRAME_INTERVAL = 0.125
WindowBase._PAUSE_MARK_ATLAS_RECT = Engine.ToIntRect(160, 64, 32, 32)
---@type sf.IntRect[]
WindowBase._PAUSE_MARK_FRAME_RECTS = {
    Engine.ToIntRect(0, 0, 16, 16), Engine.ToIntRect(16, 0, 16, 16), Engine.ToIntRect(0, 16, 16, 16),
    Engine.ToIntRect(16, 16, 16, 16)
}

function WindowBase:init(rect)
    super(WindowBase, self).init(rect)
    local windowskinName = Engine.DefaultWindowskinName
    assert(windowskinName ~= nil, "Default windowskin path is unavailable")
    self._windowSkin = assert
        (TextureManager.load(windowskinName, false, nil, true), "Default windowskin texture is unavailable")
        :copyToImage()
    self._repeated = false
    self._hasReturnBtn = false
    self._returnButtonSuppressed = false
    self._window = nil
    self.content = nil
    self._visualRoot = nil
    self._returnButton = nil
    self._pauseMark = nil
    self._pauseMarkTexture = nil
    self._gamepadHintBar = nil
    self._gamepadHintTriggered = nil
    self._uiController = nil
    self._uiDispose = nil
    self._transition = nil
    self._pauseMarkShowRequested = false
    self._pauseMarkEnabled = true
    self._pauseMarkVisiblePredicate = nil
    self._pauseMarkFrameIndex = 1
    self._pauseMarkFrameTimer = 0.0
    self:_refreshReturnButtonState()
end

---@diagnostic disable-next-line: unused
function WindowBase:onReturn()
end

function WindowBase:getHasReturnBtn()
    return self._hasReturnBtn
end

function WindowBase:setHasReturnBtn(value)
    self._hasReturnBtn = value == true
    self:_refreshReturnButtonState()
end

function WindowBase:setActive(active)
    super(WindowBase, self).setActive(active)
    self:_refreshReturnButtonState()
end

function WindowBase:setVisible(visible)
    super(WindowBase, self).setVisible(visible)
    if self._visualRoot ~= nil then
        self._visualRoot:setVisible(visible)
    end
    self:_refreshReturnButtonState()
end

function WindowBase:attachPreparedView(controller, viewParts)
    assert(self._window == nil and self.content == nil, "Window host already has a prepared view")
    if viewParts.nested then
        assert(viewParts.root:getParent() ~= nil, "Nested declarative root must already belong to its asset")
    else
        self:addChild(viewParts.root)
    end
    self._window = viewParts.windowFrame
    self.content = viewParts.content
    self._visualRoot = viewParts.root
    self._returnButton = viewParts.returnButton
    self._pauseMark = viewParts.pauseMark
    self._gamepadHintBar = viewParts.gamepadHintBar
    self._pauseMarkTexture = sf.Texture.new(self._windowSkin, false, self._PAUSE_MARK_ATLAS_RECT)
    self._pauseMarkTexture:setSmooth(false)
    self._pauseMark:setTexture(self._pauseMarkTexture, false)
    self:_bindReturnButton()
    self:_bindGamepadHintBar()
    self:_refreshReturnButtonState()
    self._uiController = controller
    self._uiDispose = controller.dispose
    self._transition = controller:createTransition(self, viewParts.transitionTarget)
end

function WindowBase:getTransition()
    return self._transition
end

function WindowBase:showWithAnimation(animationName, onReady)
    assert(self._transition ~= nil, "Window transition is unavailable")
    self._transition:show(animationName or "FadeIn", onReady)
end

function WindowBase:hideWithAnimation(animationName, onHidden)
    assert(self._transition ~= nil, "Window transition is unavailable")
    self._transition:hide(animationName or "FadeOut", onHidden)
end

function WindowBase:hideImmediate()
    if self._transition ~= nil then
        self._transition:hideImmediate()
        return
    end
    self:setActive(false)
    self:setVisible(false)
end

function WindowBase:isTransitionBlocking()
    return self._transition ~= nil and self._transition:isBlocking()
end

function WindowBase:isTransitionOpen()
    return self._transition ~= nil and self._transition:isOpen()
end

function WindowBase:isReturnButtonSuppressed()
    return self._returnButtonSuppressed
end

function WindowBase:setReturnButtonSuppressed(suppressed)
    self._returnButtonSuppressed = suppressed == true
    self:_refreshReturnButtonState()
end

function WindowBase:isReturnButtonEnabled()
    return self._hasReturnBtn and not self._returnButtonSuppressed and self:getVisible() and self:getActive()
end

function WindowBase:_refreshReturnButtonState()
    if self._returnButton == nil then
        return
    end
    local enabled = self:isReturnButtonEnabled()
    ---@cast enabled boolean
    self._returnButton:setActive(enabled)
    self._returnButton:setVisible(enabled)
end

function WindowBase:_bindReturnButton()
    ---@type Internal.UIBase.WindowBase[]
    local modelRef = setmetatable({ self }, {
        __mode = "v"
    })
    self._returnButton:addClickCallback(function (_button, kwargs)
        local model = modelRef[1]
        if model == nil or not model:isReturnButtonEnabled() then
            return
        end
        if kwargs.button ~= nil and kwargs.button ~= sf.Mouse.Button.Left then
            return
        end
        model:onReturn()
    end)
    self._returnButton:addCancelCallback(function ()
        local model = modelRef[1]
        if model ~= nil and model:isReturnButtonEnabled() then
            model:onReturn()
        end
    end)
end

function WindowBase:_bindGamepadHintBar()
    ---@type Internal.UIBase.WindowBase[]
    local modelRef = setmetatable({ self }, {
        __mode = "v"
    })
    self._gamepadHintBar:setOnHintTriggered(function (index)
        local model = modelRef[1]
        if model ~= nil and model._gamepadHintTriggered ~= nil then
            model._gamepadHintTriggered(index)
        end
    end)
end

function WindowBase:setGamepadHints(hints)
    self._gamepadHintBar:setHints(hints)
end

function WindowBase:setGamepadHintEnabled(index, enabled)
    self._gamepadHintBar:setHintEnabled(index, enabled)
end

function WindowBase:getGamepadHintCount()
    return self._gamepadHintBar:getHintCount()
end

function WindowBase:isGamepadConnected()
    return self._gamepadHintBar:isGamepadConnected()
end

function WindowBase:setOnGamepadHintTriggered(callback)
    self._gamepadHintTriggered = callback
end

function WindowBase:setPauseMarkEnabled(enabled)
    self._pauseMarkEnabled = enabled
    self:_refreshPauseMarkVisibility()
end

function WindowBase:setPauseMarkVisiblePredicate(predicate)
    self._pauseMarkVisiblePredicate = predicate
    self:_refreshPauseMarkVisibility()
end

function WindowBase:showPauseMark()
    self._pauseMarkShowRequested = true
    self:_refreshPauseMarkVisibility()
end

function WindowBase:hidePauseMark()
    self._pauseMarkShowRequested = false
    self:_refreshPauseMarkVisibility()
end

function WindowBase:onTick(deltaTime)
    super(WindowBase, self).onTick(deltaTime)
    self:_refreshReturnButtonState()
    self:_updatePauseMarkAnimation(deltaTime)
end

function WindowBase:_refreshPauseMarkVisibility()
    local visible = self._pauseMarkShowRequested and self._pauseMarkEnabled
    if visible and self._pauseMarkVisiblePredicate ~= nil then
        visible = self._pauseMarkVisiblePredicate()
    end
    self._pauseMark:setVisible(visible)
    if not visible then
        self._pauseMarkFrameIndex = 1
        self._pauseMarkFrameTimer = 0.0
        ---@diagnostic disable-next-line: need-check-nil, param-type-mismatch
        self._pauseMark:setTextureRect(self._PAUSE_MARK_FRAME_RECTS[1])
    end
end

---@param deltaTime number
function WindowBase:_updatePauseMarkAnimation(deltaTime)
    if not self._pauseMark:getVisible() then
        return
    end
    if self._pauseMarkVisiblePredicate ~= nil then
        self:_refreshPauseMarkVisibility()
        if not self._pauseMark:getVisible() then
            return
        end
    end
    self._pauseMarkFrameTimer = self._pauseMarkFrameTimer + deltaTime
    if self._pauseMarkFrameTimer < self._PAUSE_MARK_FRAME_INTERVAL then
        return
    end
    self._pauseMarkFrameTimer = self._pauseMarkFrameTimer - self._PAUSE_MARK_FRAME_INTERVAL
    self._pauseMarkFrameIndex = self._pauseMarkFrameIndex % #self._PAUSE_MARK_FRAME_RECTS + 1
    ---@diagnostic disable-next-line: need-check-nil, param-type-mismatch
    self._pauseMark:setTextureRect(self._PAUSE_MARK_FRAME_RECTS[self._pauseMarkFrameIndex])
end

function WindowBase:applyWindowSkin(windowFrame)
    windowFrame:setWindowSkin(self._windowSkin, self._repeated)
end

function WindowBase:getPauseMarkSize()
    return math.ceil(self._pauseMark:getSize().y)
end

function WindowBase:dispose()
    if self._uiDispose ~= nil then
        self._uiDispose(self._uiController)
    end
end

return class(WindowBase, Canvas)
