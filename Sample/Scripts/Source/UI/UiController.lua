local Engine = require("Engine")
local WindowTransition = require("Source.UI.WindowTransition")

---@type fun(event: string, handler: function, priority?: integer): integer
local subscribe = Engine.subscribe

---@class Source.UI.UiController
local UiController = {}

UiController.assetKey = ""
UiController.viewUpdateEvent = ""
UiController.refreshEvents = {}

function UiController:init(model, instance)
    self.model = model
    if instance == nil then
        assert(bool(self.assetKey), "UiController asset key is required")
        instance = Engine.instantiate(self.assetKey)
    end
    self.view = instance
    self.root = instance:getRoot()
    self._uiManager = nil
    self._eventSubscriptions = {}
    self._animationBindings = {}
    self._animationGenerations = {}
    self._viewLogicalSize = nil
    self._viewUpdateUnregister = nil
    self._bound = false
    self._mounted = false
    self._disposed = false
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

function UiController:refreshFromEvent(payload)
    if self._disposed == true or not self._bound then
        return
    end
    self:onViewUpdate(payload)
    self:prepare(self._viewLogicalSize)
end

function UiController:_subscribeRefreshEvent(eventName)
    ---@type Source.UI.UiController[]
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
    self.view:reflow(self._viewLogicalSize)
    return self.root
end

function UiController:attachTo(parent, logicalSize)
    local root = self:prepare(logicalSize)
    parent:addChild(root)
    return root
end

function UiController:attachWindowView(host, logicalSize, transitionTarget)
    local root = self:prepare(logicalSize)
    local chromeRoot = transitionTarget ~= nil and self:requireControl(transitionTarget) or root
    ---@cast chromeRoot Engine.Canvas
    host:attachPreparedView(self, {
        root = root,
        windowFrame = self:getWindowFrame(),
        content = self:getContent(),
        chromeRoot = chromeRoot,
        transitionTarget = transitionTarget,
        nested = false
    })
    return root
end

function UiController:attachNestedWindowView(host, logicalSize)
    local root = self:prepare(logicalSize)
    ---@cast root Engine.Canvas
    host:attachPreparedView(self, {
        root = root,
        windowFrame = self:getWindowFrame(),
        content = self:getContent(),
        chromeRoot = root,
        nested = true
    })
    return root
end

function UiController:createTransition(host, target)
    return WindowTransition.new(host, self, target)
end

function UiController:hasAnimation(name, target)
    return self.view:hasAnimation(name, target)
end

function UiController:playAnimation(name, target, onFinished)
    local key = target or ""
    local generation = (self._animationGenerations[key] or 0) + 1
    self._animationGenerations[key] = generation
    self._animationBindings[key] = { name = name, target = target }
    local started = self.view:playAnimation(name, target, function ()
        if self._disposed == true or self._animationGenerations[key] ~= generation then
            return
        end
        if onFinished ~= nil then
            onFinished()
        end
    end)
    if not started and self._animationGenerations[key] == generation then
        self._animationBindings[key] = nil
    end
    return started
end

function UiController:stopAnimation(name, target)
    local key = target or ""
    self._animationGenerations[key] = (self._animationGenerations[key] or 0) + 1
    self._animationBindings[key] = nil
    self.view:stopAnimation(name, target)
end

---@diagnostic disable-next-line: unused
function UiController:detachControl(control)
    local parent = control:getParent()
    if parent ~= nil then
        ---@cast parent Engine.Canvas
        parent:removeChild(control)
    end
end

function UiController:mount(uiManager, logicalSize)
    self:prepare(logicalSize)
    if not self._mounted then
        uiManager:loadUI(self.root)
        self._uiManager = uiManager
        self._mounted = true
    end
end

function UiController:unmount()
    if not self._mounted then
        return
    end
    ---@cast self._uiManager GlobalCore.UIManager
    local wasVisible = self.root:getVisible()
    self.root:setVisible(false)
    self._uiManager:removeUI(self.root)
    self.root:setVisible(wasVisible)
    self._uiManager = nil
    self._mounted = false
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
    return self.view:requireAsset(name)
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
    self:unmount()
    self._disposed = true
    for key, binding in pairs(self._animationBindings) do
        self._animationGenerations[key] = (self._animationGenerations[key] or 0) + 1
        self.view:stopAnimation(binding.name, binding.target)
    end
    self._animationBindings = {}
    if self._viewUpdateUnregister ~= nil then
        self._viewUpdateUnregister(self)
        self._viewUpdateUnregister = nil
    end
    for _, token in ipairs(self._eventSubscriptions) do
        Engine.unsubscribe(token)
    end
    self._eventSubscriptions = {}
end

return class(UiController)
