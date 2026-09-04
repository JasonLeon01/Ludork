local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local SourceSystem = require("Source.System")
local GameInstance = require("Source.GameInstance")
local SceneTitleUI = require("Source.UI.Title")

local ManagerFunctions = GlobalFunctions.Manager
local AudioManager = GlobalCore.AudioManager
local GlobalSystem = GlobalCore.System
local SceneBase = GlobalCore.SceneBase

---@class Source.Scenes.SceneTitle: GlobalCore.SceneBase
local Scene = {}

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    GlobalSystem.setTransition(ManagerFunctions.loadTransition("Flat.png"))
end

function Scene:onCreate()
    local WindowSaveLoad = require("Source.Windows.WindowSaveLoad")
    local ConfigWindow = require("Source.Windows.ConfigWindow")

    local uiManager = self:getUIManager()
    ---@cast uiManager GlobalCore.UIManager
    uiManager:setFocusNavigationEnabled(true)
    self._ui = SceneTitleUI.new(self)
    self._ui:mount(self:getUIManager(), GlobalSystem.getGameSize())
    self._windowCommand = self._ui:getCommandWindow()
    self._windowSaveLoad = WindowSaveLoad.new(
        nil, Engine.ToIntRect(112, 112, 160, 256), Engine.ToIntRect(272, 112, 256, 256), true, nil,
        function (reason)
            self:_onSaveLoadClose(reason)
        end,
        function (inst)
            self:_onSaveLoadLoaded(inst)
        end
    )
    self._configWindow = ConfigWindow.new(function ()
        self:_onConfigClose()
    end)
    ---@type any[]
    local uiWindows = { self._windowCommand, self._windowSaveLoad, self._configWindow }
    for _, window in ipairs(uiWindows) do
        ---@cast window Engine.ControlBase
        uiManager:loadUI(window)
    end
    self._windowCommand:setActive(false)
    self._ui:playAnimation("FadeIn", "CommandPanel", function ()
        self._ui:stopAnimation("FadeIn", "CommandPanel")
        self._windowCommand:setActive(true)
        self._windowCommand:requestKeyboardFocus()
    end)
    self._titleBGM = nil
    local titleBGMFile = SourceSystem.GetTitleBGM()
    if bool(titleBGMFile) then
        self._titleBGM = AudioManager.playMusic("BGM", titleBGMFile)
        if self._titleBGM ~= nil then
            self._titleBGM:setLooping(true)
        end
    end
end

function Scene:onQuit()
    ManagerFunctions.stopMusic("BGM")
    self._titleBGM = nil
end

function Scene:onDestroy()
    ManagerFunctions.stopMusic("BGM")
    self._titleBGM = nil
    self._windowSaveLoad:dispose()
    self._configWindow:dispose()
    self._ui:dispose()
end

---@diagnostic disable-next-line: unused
function Scene:_startGame()
    local SceneMap = require("Source.Scenes.SceneMap")

    AudioManager.playSound(SourceSystem.GetDecisionSE())
    ManagerFunctions.stopMusic("BGM")
    local nextScene = SceneMap.new()
    nextScene:setInst(GameInstance.new())
    GlobalSystem.setScene(nextScene)
end

function Scene:_onLoadCommand()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    self._windowCommand:setActive(false)
    self._windowSaveLoad:open()
end

---@param reason string
function Scene:_onSaveLoadClose(reason)
    if reason == "loaded" then
        return
    end
    self._windowCommand:setActive(true)
    self._windowCommand:requestKeyboardFocus()
end

---@param inst Source.GameInstance.GameInstance
---@diagnostic disable-next-line: unused
function Scene:_onSaveLoadLoaded(inst)
    local SceneMap = require("Source.Scenes.SceneMap")

    ManagerFunctions.stopMusic("BGM")
    local nextScene = SceneMap.new()
    nextScene:setInst(inst)
    GlobalSystem.setScene(nextScene)
end

function Scene:_onConfigCommand()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    if self._configWindow:isOpen() then
        self._configWindow:close()
    else
        self._configWindow:open()
        self._windowCommand:setActive(false)
    end
end

function Scene:_onConfigClose()
    self._windowCommand:setActive(true)
    self._windowCommand:requestKeyboardFocus()
end

---@diagnostic disable-next-line: unused
function Scene:_exitGame()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    GlobalSystem.exit()
end

return class(Scene, SceneBase)
