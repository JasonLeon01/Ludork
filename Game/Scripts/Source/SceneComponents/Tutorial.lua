local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Tutorial = require("Global.Tutorial")
local Config = require("Source.Configs.Tutorial")
local Locale = require("Source.Locale.Core")
local LazyWindow = require("Internal.UIBase.LazyWindow")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat

---@param key      string
---@param finished boolean
---@return Source.SceneComponents.Tutorial.Request
local function createRequest(key, finished)
    local request = { key = key, finished = finished }
    request.condition = function ()
        return request.finished
    end
    return request
end

---@param requests table<string, Source.SceneComponents.Tutorial.Request>
local function cancelRequests(requests)
    local latentManager = assert(Engine.latentManager)
    for _, request in pairs(requests) do
        latentManager:cancel(request.condition)
        request.finished = true
    end
end

---@class Source.SceneComponents.Tutorial
local TutorialController = {}

function TutorialController:init(scene)
    self._scene = scene
    self._queue = {}
    self._requests = {}
    self._recordedRequests = {}
    self._current = nil
    self._player = nil
    self._moveEnabled = false
    self._disposed = false
    self._window = LazyWindow.new(function ()
        local WindowTutorial = require("Source.Windows.WindowTutorial")

        return WindowTutorial.new()
    end)
    ---@type Source.SceneComponents.Tutorial[]
    local reference = setmetatable({ self }, { __mode = "v" })
    self._unregister = Tutorial.Register(scene, function (key)
        return assert(reference[1], "Tutorial controller has been disposed"):request(key)
    end)
end

function TutorialController:getScene()
    return self._scene
end

function TutorialController:request(key)
    assert(not self._disposed, "Tutorial controller has been disposed")
    assert(Config[key] ~= nil, "Unknown tutorial key: " .. tostring(key))
    if self:getScene():getGameInstance():hasTriggeredTutorial(key) then
        local recordedRequest = self._recordedRequests[key]
        if recordedRequest == nil then
            recordedRequest = createRequest(key, true)
            self._recordedRequests[key] = recordedRequest
        end
        return recordedRequest.condition
    end
    local request = self._requests[key]
    if request == nil then
        request = createRequest(key, false)
        self._requests[key] = request
        self._queue[#self._queue + 1] = request
    end
    return request.condition
end

function TutorialController:update()
    if self._disposed then
        return
    end
    local window = self._window:peek()
    if self._current ~= nil and window ~= nil and window:isFinished() then
        self._current.finished = true
        self._current = nil
    end
    if self._current ~= nil then
        return
    end
    if #self._queue == 0 then
        self:_restoreMovement()
        return
    end
    if GlobalCore.Transition.isTransitionPending() or GlobalCore.Transition.isInTransition() then
        return
    end
    local scene = self:getScene()
    local request = table.remove(self._queue, 1)
    if scene:getGameInstance():hasTriggeredTutorial(request.key) then
        request.finished = true
        return
    end
    local config = Config[request.key]
    self._window:get():open(config.rect, LOC(config.text))
    if self._player == nil then
        self._player = scene:getGameMap():getPlayer()
        if self._player ~= nil then
            self._moveEnabled = self._player:getMoveEnabled()
            self._player:setMoveEnabled(false)
        end
    end
    scene:getGameInstance():recordTriggeredTutorial(request.key)
    self._current = request
end

function TutorialController:refreshLocale()
    local window = self._window:peek()
    if self._current ~= nil and window ~= nil then
        window:refreshText(LOC(Config[self._current.key].text))
    end
end

function TutorialController:updateUI(deltaTime)
    local window = self._window:peek()
    if window ~= nil and window:getVisible() then
        window:update(deltaTime)
    end
end

function TutorialController:draw()
    local window = self._window:peek()
    if window ~= nil and window:getVisible() then
        window:render()
        GlobalCore.Graphics.draw(window)
    end
end

function TutorialController:isBlocking()
    return self._current ~= nil or #self._queue > 0
end

function TutorialController:_restoreMovement()
    if self._player ~= nil then
        if not self._player:isDestroyed() then
            self._player:setMoveEnabled(self._moveEnabled)
        end
        self._player = nil
    end
end

function TutorialController:cancel()
    local window = self._window:peek()
    if window ~= nil then
        window:close()
    end
    cancelRequests(self._requests)
    cancelRequests(self._recordedRequests)
    self._current = nil
    self._queue = {}
    self._requests = {}
    self._recordedRequests = {}
    self:_restoreMovement()
end

function TutorialController:dispose()
    self._disposed = true
    self._unregister()
    self:cancel()
    self._window:dispose()
end

return class(TutorialController)
