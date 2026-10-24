local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local LazyWindow = require("Internal.UIBase.LazyWindow")
local LocaleCore = require("Source.Locale.Core")

local AudioManager = GlobalCore.AudioManager
local Node = Engine.Node
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local MAP_INPUT_BLOCK_FRAMES = 2

---@param player     Source.MapActors.Player.Player
---@param blockInput fun()
---@return fun()
local function suspendPlayerMovement(player, blockInput)
    local moveEnabled = player:getMoveEnabled()
    local restored = false
    player:setMoveEnabled(false)
    return function ()
        if restored then
            return
        end
        restored = true
        if not player:isDestroyed() then
            player:setMoveEnabled(moveEnabled)
            blockInput()
        end
    end
end

---@param instance   Source.GameInstance.GameInstance
---@param name       string
---@param localeArgs table<string, any> | nil
---@param localVars  table<string, any>
---@return Source.Scenes.SceneMap.Dialogue.LocaleContext
local function createLocaleContext(instance, name, localeArgs, localVars)
    return {
        name = name,
        localeArgs = copy(localeArgs or {}),
        localVars = copy(localVars),
        instanceVars = copy(instance:getVariables())
    }
end

---@param text    string
---@param context Source.Scenes.SceneMap.Dialogue.LocaleContext
---@return string
local function formatText(text, context)
    text = LOC(text)
    local resolvedLocaleArgs = {}
    for key, value in pairs(context.localeArgs) do
        local resolvedValue = value
        if Class.isInstance(value, "string") then
            resolvedValue = LOC(value)
        end
        resolvedLocaleArgs[key] = resolvedValue
    end
    text = Engine.ApplyStringMappingFormat(text, resolvedLocaleArgs)
    text = Engine.ApplyStringMappingFormat(text, context.localVars)
    text = Engine.ApplyStringMappingFormat(text, context.instanceVars)
    return text
end

---@param source Source.Scenes.SceneMap.Dialogue.MessageLocaleSource
---@return string, string
local function formatMessage(source)
    return formatText(source.context.name, source.context), formatText(source.content, source.context)
end

---@param source Source.Scenes.SceneMap.Dialogue.SelectionLocaleSource
---@return string, string[]
local function formatSelection(source)
    local options = {}
    for _, option in ipairs(source.content) do
        options[#options + 1] = formatText(option, source.context)
    end
    return formatText(source.context.name, source.context), options
end

---@param self Source.Scenes.SceneMap.Dialogue
local function release(self)
    local released = { voice = self._voice, restore = self._restoreMovement }
    self._voice = nil
    self._restoreMovement = nil
    self._localeSource = nil
    if released.voice ~= nil then
        released.voice:stop()
    end
    if released.restore ~= nil then
        released.restore()
    end
end

---@class Source.Scenes.SceneMap.Dialogue
local Dialogue = {}

function Dialogue:init(scene)
    self._scene = scene
    self._operation = nil
    self._voice = nil
    self._restoreMovement = nil
    self._localeSource = nil
    self._window = LazyWindow.new(function ()
        local WindowMessage = require("Source.Windows.WindowMessage")

        local manager = assert(self._scene:getUIManager(), "Scene map UI manager is unavailable")
        local window = WindowMessage.new()
        window:mount(manager)
        return window
    end)
end

function Dialogue.CaptureLocalVars(nodeFunction)
    local refLocal = Node.getRefLocal(nodeFunction)
    if not bool(refLocal) or refLocal.__activeNodeFunction__ ~= nodeFunction then
        return {}
    end
    local result = {}
    for key, value in pairs(refLocal) do
        if Class.isInstance(key, "string") and key:sub(1, 2) ~= "__" then
            result[key] = value
        end
    end
    return result
end

function Dialogue:showMessage(name, message, refActor, localeArgs, localVars)
    self:cancel()
    local refPosition = nil
    if refActor ~= nil then
        refPosition = self._scene:getGameMap():worldToUIScreenPosition(refActor:getPosition())
    end
    local restoreMove = suspendPlayerMovement(self._scene:getPlayer(), function ()
        self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end)
    ---@type Source.Scenes.SceneMap.Dialogue.MessageLocaleSource
    local source = {
        kind = "message",
        context = createLocaleContext(self._scene:getGameInstance(), name, localeArgs, localVars),
        content = message
    }
    local formattedName, formattedMessage = formatMessage(source)
    local window = self._window:get()
    local operation = Engine.AsyncOperation.new()
    self._operation = operation
    self._restoreMovement = restoreMove
    self._localeSource = source
    window:setMessage(refPosition, formattedName, formattedMessage, function ()
        if self._operation ~= operation then
            return
        end
        self._operation = nil
        release(self)
        operation:complete(true)
    end)
    return operation
end

function Dialogue:showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial, localVars)
    local operation = self:showMessage(name, message, refActor, nil, localVars)
    if spatial then
        self._voice = AudioManager.playVoice(voiceFileName, nil, refActor, minDistance)
    else
        self._voice = AudioManager.playVoice(voiceFileName)
    end
    return operation
end

function Dialogue:showSelection(name, options, refActor, allowCancel, localeArgs, localVars)
    self:cancel()
    if allowCancel == nil then
        allowCancel = true
    end
    local refPosition = nil
    if refActor ~= nil then
        refPosition = self._scene:getGameMap():worldToUIScreenPosition(refActor:getPosition())
    end
    local restoreMove = suspendPlayerMovement(self._scene:getPlayer(), function ()
        self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end)
    ---@type Source.Scenes.SceneMap.Dialogue.SelectionLocaleSource
    local source = {
        kind = "selection",
        context = createLocaleContext(self._scene:getGameInstance(), name, localeArgs, localVars),
        content = copy(options)
    }
    local formattedName, formattedOptions = formatSelection(source)
    local window = self._window:get()
    local operation = Engine.AsyncOperation.new()
    self._operation = operation
    self._restoreMovement = restoreMove
    self._localeSource = source
    window:setSelection(refPosition, formattedName, formattedOptions, allowCancel, function ()
        if self._operation ~= operation then
            return
        end
        local result = window:getSelectionResult()
        assert(result ~= nil, "Selection finished without a result")
        self._operation = nil
        release(self)
        operation:complete(result)
    end)
    return operation
end

function Dialogue:isBlocking()
    local window = self._window:peek()
    return window ~= nil and window:isInDialogue()
end

function Dialogue:update()
    if self._localeSource ~= nil and not self:isBlocking() then
        self._localeSource = nil
    end
end

function Dialogue:refreshLocale()
    local window = self._window:peek()
    if self._localeSource == nil or window == nil or not window:isInDialogue() then
        return
    end
    if self._localeSource.kind == "selection" then
        ---@cast self._localeSource Source.Scenes.SceneMap.Dialogue.SelectionLocaleSource
        local name, options = formatSelection(self._localeSource)
        window:refreshSelection(name, options)
    else
        ---@cast self._localeSource Source.Scenes.SceneMap.Dialogue.MessageLocaleSource
        local name, message = formatMessage(self._localeSource)
        window:refreshMessage(name, message)
    end
end

function Dialogue:cancel()
    local cancelled = { operation = self._operation }
    self._operation = nil
    if cancelled.operation ~= nil then
        cancelled.operation:cancel()
    end
    local window = self._window:peek()
    if window ~= nil then
        window:cancelDialogue()
    end
    release(self)
end

function Dialogue:dispose()
    self:cancel()
    self._window:dispose()
end

return class(Dialogue)
