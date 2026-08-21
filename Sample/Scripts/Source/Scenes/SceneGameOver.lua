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
    GlobalSystem.setTransition(nil, 3.0)
end

function Scene:onCreate()
    self._ui = GameOverUI.new()
    self._ui:mount(self:getUIManager(), GlobalSystem.getGameSize())
end

function Scene:onTick(_)
    if Input.isActionTriggered(Input.getConfirmKeys(), true) then
        self:_backToTitle()
    end
end

function Scene:onDestroy()
    self._ui:dispose()
end

---@diagnostic disable-next-line: unused
function Scene:_backToTitle()
    local SceneTitle = require("Source.Scenes.SceneTitle")

    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    GlobalSystem.setScene(SceneTitle.new())
end

return class(Scene, SceneBase)
