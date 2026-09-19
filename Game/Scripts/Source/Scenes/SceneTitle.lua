local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local SourceSystem = require("Source.System")
local GameInstance = require("Source.GameInstance")
local SceneTitleController = require("Source.Scenes.SceneTitle.Controller")
local LazyWindow = require("Source.UIBase.LazyWindow")

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
    local uiManager = self:getUIManager()
    ---@cast uiManager GlobalCore.UIManager
    uiManager:setFocusNavigationEnabled(true)
    self._ui = SceneTitleController.new(self)
    self._ui:mount(self:getUIManager(), GlobalSystem.getGameSize())
    self._windowCommand = self._ui:getCommandWindow()
    self._windowSaveLoad = LazyWindow.new(function ()
        local constructionClock = sf.Clock.new()
        local WindowSaveLoad = require("Source.Windows.WindowSaveLoad")

        local window = WindowSaveLoad.new(
            true, nil,
            function (reason)
                self:_onSaveLoadClose(reason)
            end,
            function (inst)
                self:_onSaveLoadLoaded(inst)
            end
        )
        window:mount(assert(self:getUIManager()))
        Logging.info("Save window construction: %.2f ms", constructionClock:getElapsedTime():asMicroseconds() / 1000)
        return window
    end)
    self._configWindow = LazyWindow.new(function ()
        local ConfigWindow = require("Source.Windows.ConfigWindow")

        local window = ConfigWindow.new(function ()
            self:_onConfigClose()
        end)
        window:mount(assert(self:getUIManager()))
        return window
    end)
    uiManager:loadUI(self._windowCommand)
    self._windowCommand:setActive(false)
    self._ui:playAnimation("FadeIn", "CommandPanel", function ()
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
function Scene:startGame()
    local SceneMap = require("Source.Scenes.SceneMap")

    AudioManager.playSound(SourceSystem.GetDecisionSE())
    ManagerFunctions.stopMusic("BGM")
    local nextScene = SceneMap.new()
    nextScene:setInst(GameInstance.new())
    GlobalSystem.setScene(nextScene)
end

function Scene:openLoad()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    self._windowCommand:setActive(false)
    self._windowSaveLoad:get():open()
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

function Scene:toggleConfig()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    local window = self._configWindow:peek()
    if window ~= nil and window:isOpen() then
        window:close()
    else
        self._configWindow:get():open()
        self._windowCommand:setActive(false)
    end
end

function Scene:_onConfigClose()
    self._windowCommand:setActive(true)
    self._windowCommand:requestKeyboardFocus()
end

---@diagnostic disable-next-line: unused
function Scene:exitGame()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    GlobalSystem.exit()
end

return class(Scene, SceneBase)
