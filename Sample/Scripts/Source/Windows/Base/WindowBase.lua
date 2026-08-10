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

function WindowBase:init(rect, windowSkin, repeated)
    super(WindowBase, self).init(rect)
    if windowSkin == nil then
        windowSkin = ManagerFunctions.loadSystem(Engine.DefaultWindowskinName, false, nil, true):copyToImage()
    end
    self._windowSkin = windowSkin
    self._repeated = repeated == true
    self._windowBaseUI = WindowBaseUI.new(self, windowSkin, repeated)
    local size = self:getSize()
    self._windowBaseUI:attachTo(self, sf.Vector2u.new(size.x, size.y))
    self._window = self._windowBaseUI:getWindow()
    self.content = self._windowBaseUI:getContent()
    self._pauseMarkShowRequested = false
    self._pauseMarkEnabled = true
    self._pauseMarkVisiblePredicate = nil
    self._pauseMarkFrameIndex = 1
    self._pauseMarkFrameTimer = 0.0
    self._pauseMark = self._windowBaseUI:getPauseMark()
    self._pauseMarkTexture = self._windowBaseUI:getPauseMarkTexture()
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
        local frameRect = self._PAUSE_MARK_FRAME_RECTS[1]
        ---@cast frameRect - nil
        self._pauseMark:setTextureRect(frameRect)
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
    local frameRect = self._PAUSE_MARK_FRAME_RECTS[self._pauseMarkFrameIndex]
    ---@cast frameRect - nil
    self._pauseMark:setTextureRect(frameRect)
end

return class(WindowBase, Canvas)
