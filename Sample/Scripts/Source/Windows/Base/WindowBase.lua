local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local WindowBaseUI = require("Source.UI.Parts.Shared.WindowBase")

local Canvas = Engine.Canvas
local ManagerFunctions = GlobalFunctions.Manager

---@class Source.Windows.Base.WindowBase
local WindowBase = {}

WindowBase._PAUSE_MARK_SIZE = 16
WindowBase._PAUSE_MARK_Y_OFFSET = 4
WindowBase._PAUSE_MARK_FRAME_INTERVAL = 0.125
WindowBase._PAUSE_MARK_ATLAS_RECT = Engine.ToIntRect(160, 64, 32, 32)
---@type sf.IntRect[]
WindowBase._PAUSE_MARK_FRAME_RECTS = {
    Engine.ToIntRect(0, 0, 16, 16), Engine.ToIntRect(16, 0, 16, 16), Engine.ToIntRect(0, 16, 16, 16),
    Engine.ToIntRect(16, 16, 16, 16)
}

function WindowBase:init(rect, windowSkin, repeated, deferView)
    super(WindowBase, self).init(rect)
    if windowSkin == nil then
        windowSkin = ManagerFunctions.loadSystem(Engine.DefaultWindowskinName, false, nil, true):copyToImage()
    end
    self._windowSkin = windowSkin
    self._repeated = repeated == true
    self._hasReturnBtn = false
    self._returnButtonSuppressed = false
    self._windowBaseUI = nil
    self._window = nil
    self.content = nil
    self._returnButton = nil
    self._pauseMark = nil
    self._pauseMarkTexture = nil
    if deferView == true then
        self:_createDeclarativeChrome()
    else
        self._windowBaseUI = WindowBaseUI.new(self, windowSkin, repeated)
        local size = self:getSize()
        self._windowBaseUI:attachTo(self, sf.Vector2u.new(size.x, size.y))
        self._window = self._windowBaseUI:getWindow()
        self.content = self._windowBaseUI:getContent()
        self._returnButton = self._windowBaseUI:getReturnButton()
        self._pauseMark = self._windowBaseUI:getPauseMark()
        self._pauseMarkTexture = self._windowBaseUI:getPauseMarkTexture()
    end
    ---@cast self._returnButton Engine.Button
    ---@cast self._pauseMark Engine.Image
    ---@cast self._pauseMarkTexture sf.Texture
    self:_bindReturnButton()
    self._pauseMarkShowRequested = false
    self._pauseMarkEnabled = true
    self._pauseMarkVisiblePredicate = nil
    self._pauseMarkFrameIndex = 1
    self._pauseMarkFrameTimer = 0.0
    self:_refreshReturnButtonState()
end

function WindowBase:_createDeclarativeChrome()
    local returnTexture = assert(
        ManagerFunctions.loadTexture("System", "ReturnButton.png"), "Return button texture is unavailable"
    )
    self._returnButton = Engine.Button.new(
        returnTexture, nil, sf.Color.new(238, 246, 255, 255), sf.Color.new(205, 220, 238, 255)
    )
    self._returnButton:setVisible(false)
    self._returnButton:setActive(false)
    self._pauseMarkTexture = sf.Texture.new(self._windowSkin, false, self._PAUSE_MARK_ATLAS_RECT)
    self._pauseMarkTexture:setSmooth(false)
    self._pauseMark = Engine.Image.new(self._pauseMarkTexture, self._PAUSE_MARK_FRAME_RECTS[1])
    self._pauseMark:setVisible(false)
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
    self:_refreshReturnButtonState()
end

function WindowBase:_setReturnButtonSuppressed(suppressed)
    self._returnButtonSuppressed = suppressed == true
    self:_refreshReturnButtonState()
end

function WindowBase:_canUseReturnButton()
    return self._hasReturnBtn and not self._returnButtonSuppressed and self:getVisible() and self:getActive()
end

function WindowBase:_refreshReturnButtonState()
    if self._returnButton == nil then
        return
    end
    local enabled = self:_canUseReturnButton()
    ---@cast enabled boolean
    self._returnButton:setActive(enabled)
    self._returnButton:setVisible(enabled)
end

function WindowBase:_bindReturnButton()
    ---@type Source.Windows.Base.WindowBase[]
    local modelRef = setmetatable({ self }, {
        __mode = "v"
    })
    self._returnButton:addConfirmCallback(function ()
        local model = modelRef[1]
        if model ~= nil and model:_canUseReturnButton() then
            model:onReturn()
        end
    end)
    self._returnButton:addMouseButtonDownCallback(function (button, kwargs)
        local model = modelRef[1]
        ---@cast button Engine.Button
        if model == nil or not model:_canUseReturnButton() or kwargs.button ~= sf.Mouse.Button.Left then
            return false
        end
        local position = sf.Vector2f.new(kwargs.position.x, kwargs.position.y)
        local bounds = button:getAbsoluteBounds()
        ---@cast bounds sf.FloatRect
        if not sf.FloatRect.contains(bounds, position) then
            return false
        end
        model:onReturn()
        return true
    end)
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

function WindowBase:refreshPauseMarkLayout()
    local contentSize = self.content:getSize()
    local posX = (contentSize.x - self._PAUSE_MARK_SIZE) / 2.0
    local posY = contentSize.y - self._PAUSE_MARK_SIZE + self._PAUSE_MARK_Y_OFFSET
    self._pauseMark:setPosition(sf.Vector2f.new(posX, posY))
    self:_bringPauseMarkToFront()
end

function WindowBase:_bringPauseMarkToFront()
    if self._pauseMark:getParent() == self.content then
        self.content:removeChild(self._pauseMark)
        self.content:addChild(self._pauseMark)
    end
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

return class(WindowBase, Canvas)
