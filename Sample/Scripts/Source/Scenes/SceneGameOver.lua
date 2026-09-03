local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local GameOverUI = require("Source.UI.GameOver")

local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager
local SceneBase = GlobalCore.SceneBase
local GlobalSystem = GlobalCore.System

local Scene = {}

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    GlobalSystem.setTransition(nil, 0.0)
end

function Scene:onCreate()
    self._phase = "entering"
    self._ui = GameOverUI.new()
    self._ui:mount(self:getUIManager(), GlobalSystem.getGameSize())
    self._ui:playAnimation("FadeIn", nil, function ()
        self._ui:stopAnimation("FadeIn")
        self._phase = "open"
    end)
end

function Scene:onTick(_)
    ---@cast self._phase string
    if self._phase == "open" and Input.isActionTriggered(Input.getConfirmKeys(), true) then
        self:_backToTitle()
    end
end

function Scene:onDestroy()
    self._ui:dispose()
end

---@diagnostic disable-next-line: unused
function Scene:_backToTitle()
    ---@cast self._phase string
    if self._phase ~= "open" then
        return
    end
    local SceneTitle = require("Source.Scenes.SceneTitle")

    self._phase = "exiting"
    ManagerFunctions.playSE(GameSystem.GetDecisionSE())
    self._ui:playAnimation("FadeOut", nil, function ()
        self._ui:stopAnimation("FadeOut")
        GlobalSystem.setScene(SceneTitle.new())
    end)
end

return class(Scene, SceneBase)
