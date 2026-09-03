local GlobalCore = require("GlobalCore")
local EventKeys = require("Source.Configs.EventKeys")
local Locale = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

local GlobalSystem = GlobalCore.System
---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat

---@class Source.UI.RegionTitle: Source.UI.UiController
local RegionTitleUI = {}

RegionTitleUI.refreshEvents = { EventKeys.LocaleChanged }

function RegionTitleUI:init(logicalSize)
    self._logicalSize = logicalSize
    self._region = nil
    self._showing = false
    super(RegionTitleUI, self).init(nil, nil)
end

function RegionTitleUI:bind()
    self._text = self:requireControl("RegionTitle")
end

function RegionTitleUI:refresh()
    if self._region == nil then
        self:setText("RegionTitle", "")
        self:setProperty("RegionTitle", "visible", false)
        return
    end
    ---@cast self._region string
    self:setText("RegionTitle", LOC(self._region))
    self:setProperty("RegionTitle", "visible", self._showing)
end

function RegionTitleUI:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    return super(RegionTitleUI, self).prepare(self._logicalSize)
end

function RegionTitleUI:onViewUpdate(payload)
    if payload.region == nil then
        return
    end
    self._region = payload.region
    self._showing = true
    self:playAnimation("Display", "RegionTitle", function ()
        self._showing = false
        self:setProperty("RegionTitle", "visible", false)
        self:stopAnimation("Display", "RegionTitle")
    end)
end

function RegionTitleUI:update(deltaTime)
    if not self._text:getVisible() or GlobalSystem.isTransitionPending() or GlobalSystem.isInTransition() then
        return
    end
    ---@cast self.root Engine.Canvas
    self.root:update(deltaTime)
end

function RegionTitleUI:getVisible()
    return self._text:getVisible()
end

function RegionTitleUI:getText()
    return self._text
end

function RegionTitleUI:draw()
    ---@cast self.root Engine.Canvas
    self.root:render()
    GlobalSystem.draw(self.root)
end

return Ui.Define("RegionTitle", RegionTitleUI)
