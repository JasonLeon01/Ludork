local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local GameOverController = require("Source.Scenes.SceneGameOver.Controller")

local Display = GlobalCore.Display
local SceneManager = GlobalCore.SceneManager
local Transition = GlobalCore.Transition
local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager
local SceneBase = GlobalCore.SceneBase

local Scene = {}

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    Transition.setTransition(nil, 0.0)
end

function Scene:onCreate()
    self._phase = "entering"
    self._ui = GameOverController.new()
    self._ui:mount(self:getUIManager(), Display.getGameSize())
    self._ui:playAnimation("FadeIn", nil, function ()
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
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self._ui:playAnimation("FadeOut", nil, function ()
        SceneManager.setScene(SceneTitle.new())
    end)
end

return class(Scene, SceneBase)
