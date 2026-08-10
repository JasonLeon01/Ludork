local GlobalCore = require("GlobalCore")
local Locale = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

local GlobalSystem = GlobalCore.System
local LOC = Locale.ApplyStringLocaleFormat

local _HOLD_TIME = 1.0
local _FADE_TIME = 1.0

---@class Source.UI.RegionTitle: Source.UI.UiController
local RegionTitleUI = {}

function RegionTitleUI:init(logicalSize)
    self._logicalSize = logicalSize
    self._region = nil
    self._elapsed = _HOLD_TIME + _FADE_TIME
    super(RegionTitleUI, self).init(nil, nil)
end

function RegionTitleUI:bind()
    self._text = self:requireControl("RegionTitle")
end

function RegionTitleUI:refresh()
    local visible = self._region ~= nil and self._elapsed < _HOLD_TIME + _FADE_TIME
    local region = self._region
    ---@cast region string
    self:setText("RegionTitle", visible and LOC(region) or "")
    self:setProperty("RegionTitle", "visible", visible)
    if not visible then
        return
    end
    local alpha = 255
    if self._elapsed > _HOLD_TIME then
        alpha = math.floor(255 * (1.0 - (self._elapsed - _HOLD_TIME) / _FADE_TIME))
    end
    self:setProperty("RegionTitle", "colour", {
        255,
        255,
        255,
        alpha
    })
end

function RegionTitleUI:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    return super(RegionTitleUI, self).prepare(self._logicalSize)
end

function RegionTitleUI:onViewUpdate(payload)
    self._region = payload.region
    self._elapsed = 0.0
end

function RegionTitleUI:update(deltaTime)
    if not self._text:getVisible() or GlobalSystem.isTransitionPending() or GlobalSystem.isInTransition() then
        return
    end
    self._elapsed = self._elapsed + deltaTime
    if self._elapsed <= _HOLD_TIME then
        return
    end
    local fadeElapsed = self._elapsed - _HOLD_TIME
    if fadeElapsed >= _FADE_TIME then
        self:setProperty("RegionTitle", "visible", false)
        return
    end
    local alpha = math.floor(255 * (1.0 - fadeElapsed / _FADE_TIME))
    self:setProperty("RegionTitle", "colour", {
        255,
        255,
        255,
        alpha
    })
end

function RegionTitleUI:getVisible()
    return self._text:getVisible()
end

function RegionTitleUI:getText()
    return self._text
end

function RegionTitleUI:draw()
    local root = self.root
    ---@cast root Engine.Canvas
    root:render()
    GlobalSystem.draw(root)
end

return Ui.define("RegionTitle", RegionTitleUI)
