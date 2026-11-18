local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local SourceSystem = require("Source.System")
local GameInstance = require("Source.GameInstance")
local SceneTitleController = require("Source.Scenes.SceneTitle.Controller")
local LazyWindow = require("Internal.UIBase.LazyWindow")
local SaveLoadCloseReason = require("Enums.SaveLoadCloseReason")

local Display = GlobalCore.Display
local SceneManager = GlobalCore.SceneManager
local Transition = GlobalCore.Transition
local ManagerFunctions = GlobalFunctions.Manager
local AudioManager = GlobalCore.AudioManager
local GlobalSystem = GlobalCore.System
local SceneBase = GlobalCore.SceneBase

---@class Source.Scenes.SceneTitle: GlobalCore.SceneBase
local Scene = {}

---@param inst Source.GameInstance.GameInstance | nil
---@return Source.Scenes.SceneMap.SceneMap
local function createMapScene(inst)
    local SceneMap = require("Source.Scenes.SceneMap")

    local nextScene = SceneMap.new()
    nextScene:setInst(inst or GameInstance.new())
    return nextScene
end

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    Transition.setTransition(ManagerFunctions.loadTransition("Flat.png"))
end

function Scene:onCreate()
    local uiManager = self:getUIManager()
    ---@cast uiManager GlobalCore.UIManager
    uiManager:setFocusNavigationEnabled(true)
    self._ui = SceneTitleController.new(self)
    self._ui:mount(self:getUIManager(), Display.getGameSize())
    self._windowCommand = self._ui:getCommandWindow()
    self._windowSaveLoad = LazyWindow.new(function ()
        local constructionClock = sf.Clock.new()
        local WindowSaveLoad = require("Source.Windows.WindowSaveLoad")

        local window = WindowSaveLoad.new(
            true, nil,
            function (reason)
                self:_onSaveLoadClose(reason)
            end,
            function (inst, cue)
                self:_onSaveLoadLoaded(inst, cue)
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
    self:_cancelMapEntry()
    self:_stopTitleMusic()
end

function Scene:onDestroy()
    self:_cancelMapEntry()
    self:_stopTitleMusic()
    self._windowSaveLoad:dispose()
    self._configWindow:dispose()
    self._ui:dispose()
end

---@diagnostic disable-next-line: unused
function Scene:startGame()
    self:_startMapEntry(nil)
end

function Scene:openLoad()
    AudioManager.playSound(SourceSystem.GetDecisionSE())
    self._windowCommand:setActive(false)
    self._windowSaveLoad:get():open()
end

---@param reason Enums.SaveLoadCloseReason
function Scene:_onSaveLoadClose(reason)
    if reason == SaveLoadCloseReason.LOADED then
        return
    end
    self._windowCommand:setActive(true)
    self._windowCommand:requestKeyboardFocus()
end

---@param inst Source.GameInstance.GameInstance
---@param cue  sf.Sound | nil
---@diagnostic disable-next-line: unused
function Scene:_onSaveLoadLoaded(inst, cue)
    if PLATFORM == "web" and cue ~= nil then
        cue:stop()
    end
    self:_startMapEntry(inst)
end

function Scene:_stopTitleMusic()
    ManagerFunctions.stopMusic("BGM")
    self._titleBGM = nil
end

---@param inst Source.GameInstance.GameInstance | nil
function Scene:_startMapEntry(inst)
    if self._mapEntryTask ~= nil then
        return
    end
    local cue = inst == nil and AudioManager.playSound(SourceSystem.GetDecisionSE()) or nil
    self:_stopTitleMusic()
    if PLATFORM ~= "web" then
        SceneManager.setScene(createMapScene(inst))
        return
    end
    self._mapEntryCue = cue
    self._windowCommand:setActive(false)
    ---@async
    local function prepareEntry()
        asyncio.sleep(0.0)
        local nextScene = createMapScene(inst)
        asyncio.sleep(0.0)
        nextScene:prepareInitialMap()
        self._mapEntryTask = nil
        SceneManager.setScene(nextScene)
    end
    self._mapEntryTask = asyncio.create_required_task(prepareEntry)
end

function Scene:_cancelMapEntry()
    if self._mapEntryTask ~= nil then
        asyncio.cancel_task(self._mapEntryTask)
        self._mapEntryTask = nil
    end
    if self._mapEntryCue ~= nil then
        self._mapEntryCue:stop()
        self._mapEntryCue = nil
    end
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
