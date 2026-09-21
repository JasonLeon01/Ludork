local GlobalCore = require("GlobalCore")
local EventKeys = require("Source.Configs.EventKeys")
local Locale = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.RegionTitle")

local Graphics = GlobalCore.Graphics
local Transition = GlobalCore.Transition
---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat

---@class Source.Scenes.SceneMap.RegionTitle.Controller: Source.UIBase.UiController
local RegionTitleController = {}

RegionTitleController.refreshEvents = { EventKeys.LocaleChanged }

function RegionTitleController:init(logicalSize)
    self._logicalSize = logicalSize
    self._region = nil
    self._showing = false
    super(RegionTitleController, self).init(nil, nil)
end

function RegionTitleController:refresh()
    if self._region == nil then
        self:setText("RegionTitle", "")
        self:setProperty("RegionTitle", "visible", false)
        return
    end
    ---@cast self._region string
    self:setText("RegionTitle", LOC(self._region))
    self:setProperty("RegionTitle", "visible", self._showing)
end

function RegionTitleController:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    return super(RegionTitleController, self).prepare(self._logicalSize)
end

function RegionTitleController:onViewUpdate(payload)
    if payload.region == nil then
        return
    end
    self._region = payload.region
    self._showing = true
    self:playAnimation("Display", "RegionTitle", function ()
        self._showing = false
        self:setProperty("RegionTitle", "visible", false)
    end)
end

function RegionTitleController:update(deltaTime)
    if not self.ui.controls["RegionTitle"]:getVisible() or Transition.isTransitionPending()
        or Transition.isInTransition() then
        return
    end
    ---@cast self.root Engine.Canvas
    self.root:update(deltaTime)
end

function RegionTitleController:getVisible()
    return self.ui.controls["RegionTitle"]:getVisible()
end

function RegionTitleController:getText()
    return self.ui.controls["RegionTitle"]
end

function RegionTitleController:draw()
    ---@cast self.root Engine.Canvas
    self.root:render()
    Graphics.draw(self.root)
end

return Ui.Define(View, RegionTitleController)
