local Engine = require("Engine")
local UiObserver = require("Internal.UIBase.UiObserver")

---@type fun(event: string, handler: function, priority?: integer): integer
local subscribe = Engine.subscribe

---@class Internal.UIBase.UiController
local UiController = {}

UiController.assetKey = ""
UiController.viewClass = nil
UiController.viewUpdateEvent = ""
UiController.refreshEvents = {}

function UiController:init(model, ui)
    self.model = model
    assert(self.viewClass ~= nil, "UiController generated View class is required")
    if ui ~= nil then
        assert(Class.isInstance(ui, self.viewClass), "UiController requires its generated View type")
    end
    self.ui = ui or self.viewClass.new()
    self.ui:bindController(self)
    self.view = self.ui.instance
    self.root = self.ui.root
    self._eventSubscriptions = {}
    self._callbackTarget = setmetatable({ self }, { __mode = "v" })
    self._observer = UiObserver.new(self)
    self._observerDispose = self._observer.dispose
    self._viewLogicalSize = nil
    self._viewUpdateUnregister = nil
    self._bound = false
    self._disposed = false
end

function UiController:createChild(name, windowClass, ...)
    return windowClass.FromView(assert(self.ui.assets[name], "UI asset not found: " .. name), ...)
end

---@diagnostic disable-next-line: unused
function UiController:ready()
end

---@diagnostic disable-next-line: unused
function UiController:bind()
end

---@diagnostic disable-next-line: unused
function UiController:refresh()
end

---@diagnostic disable-next-line: unused
function UiController:onViewUpdate(_)
end

---@diagnostic disable-next-line: unused
function UiController:registerUiInstance()
end

function UiController:subscribe(eventName, handler, priority)
    if priority == nil then
        priority = 0
    end
    local token = subscribe(eventName, handler, priority)
    self._eventSubscriptions[#self._eventSubscriptions + 1] = token
    return token
end

function UiController:watch(target, field, method, immediate)
    return self._observer:watch(target, field, method, immediate)
end

function UiController:bindCallback(method)
    local target = assert(self._callbackTarget, "UiController callbacks require a live Controller")
    return function (...)
        local controller = target[1]
        if controller ~= nil then
            return method(controller, ...)
        end
    end
end

function UiController:refreshFromEvent(payload)
    if self._disposed == true or not self._bound then
        return
    end
    self:onViewUpdate(payload)
    self:prepare(self._viewLogicalSize)
end

function UiController:_subscribeRefreshEvent(eventName)
    ---@type Internal.UIBase.UiController[]
    local weakController = setmetatable({
        self
    },
        {
            __mode = "v"
        })
    ---@type integer | nil
    local token
    token = subscribe(eventName, function (payload)
        local controller = weakController[1]
        if controller == nil then
            local subscriptionToken = token
            ---@cast subscriptionToken - nil
            Engine.unsubscribe(subscriptionToken)
            return
        end
        controller:refreshFromEvent(payload)
    end)
    self._eventSubscriptions[#self._eventSubscriptions + 1] = token
end

function UiController:_bindViewUpdates()
    for _, eventName in ipairs(self.refreshEvents) do
        self:_subscribeRefreshEvent(eventName)
    end
end

function UiController:setViewUpdateUnregister(unregister)
    self._viewUpdateUnregister = unregister
end

function UiController:prepare(logicalSize)
    assert(self._disposed ~= true, "Disposed UiController cannot be prepared")
    if logicalSize ~= nil then
        self._viewLogicalSize = logicalSize
    end
    if not self._bound then
        self:bind()
        self:_bindViewUpdates()
        self:registerUiInstance()
        self._bound = true
    end
    self:refresh()
    return self.ui:prepare(self._viewLogicalSize)
end

function UiController:attachTo(parent, logicalSize)
    self:prepare(logicalSize)
    return self.ui:attachPreparedTo(parent)
end

function UiController:getWindowFrame()
    return self.ui:getWindowFrame()
end

function UiController:getContent()
    return self.ui:getContent()
end

function UiController:attachWindowView(host, logicalSize, transitionTarget)
    self:prepare(logicalSize)
    return self.ui:attachPreparedWindow(host, self:getWindowFrame(), self:getContent(), false, transitionTarget)
end

function UiController:attachNestedWindowView(host, logicalSize)
    self:prepare(logicalSize)
    return self.ui:attachPreparedWindow(host, self:getWindowFrame(), self:getContent(), true)
end

function UiController:createCollection(container, controllerClass)
    return self.ui:createCollection(container, controllerClass)
end

function UiController:createTransition(host, target)
    return self.ui:createTransition(host, target)
end

function UiController:hasAnimation(name, target)
    return self.ui:hasAnimation(name, target)
end

function UiController:playAnimation(name, target, onFinished)
    return self.ui:playAnimation(name, target, onFinished)
end

function UiController:stopAnimation(name, target)
    self.ui:stopAnimation(name, target)
end

function UiController:detachControl(control)
    self.ui:detachControl(control)
end

function UiController:mount(uiManager, logicalSize)
    self:prepare(logicalSize)
    self.ui:mountPrepared(uiManager)
end

function UiController:unmount()
    self.ui:unmount()
end

function UiController:getView()
    return self.view
end

function UiController:getRoot()
    return self.root
end

function UiController:requireControl(name)
    return self.view:requireControl(name)
end

function UiController:getNodeByName(name)
    return self.view:getNodeByName(name)
end

function UiController:requireAsset(name)
    return assert(self.ui.assets[name], "UI asset not found: " .. name)
end

function UiController:setProperty(name, propertyId, value)
    self.view:setProperty(name, propertyId, value)
end

function UiController:setText(name, text)
    self.view:setText(name, text)
end

function UiController:dispose()
    if self._disposed == true then
        return
    end
    self._disposed = true
    self._observerDispose(self._observer)
    if self._callbackTarget ~= nil then
        self._callbackTarget[1] = nil
    end
    self._callbackTarget = nil
    if self._viewUpdateUnregister ~= nil then
        self._viewUpdateUnregister(self)
        self._viewUpdateUnregister = nil
    end
    for _, token in ipairs(self._eventSubscriptions) do
        Engine.unsubscribe(token)
    end
    self._eventSubscriptions = {}
    self.ui:dispose()
    if self._hostDispose ~= nil then
        self._hostDispose(self.host)
    end
end

return class(UiController)
