local Engine = require("Engine")
local WindowTransition = require("Source.UI.WindowTransition")

---@type fun(event: string, handler: function, priority?: integer): integer
local subscribe = Engine.subscribe

---@class Source.UI.UiController
---@field _refreshFromEvent fun(self: Source.UI.UiController, payload: any)
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
function UiController:_registerUiInstance()
end

function UiController:subscribe(eventName, handler, priority)
    if priority == nil then
        priority = 0
    end
    local token = subscribe(eventName, handler, priority)
    self._eventSubscriptions[#self._eventSubscriptions + 1] = token
    return token
end

function UiController:_refreshFromEvent(payload)
    if self._disposed == true then
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
        controller:_refreshFromEvent(payload)
    end)
    self._eventSubscriptions[#self._eventSubscriptions + 1] = token
end

function UiController:_bindViewUpdates()
    for _, eventName in ipairs(self.refreshEvents) do
        self:_subscribeRefreshEvent(eventName)
    end
end

function UiController:_setViewUpdateUnregister(unregister)
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
        self:_registerUiInstance()
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
    return self:_attachWindowRoot(host, self:prepare(logicalSize), transitionTarget)
end

function UiController:attachNestedWindowView(host, logicalSize)
    local root = self:prepare(logicalSize)
    ---@cast root Engine.Canvas
    assert(root:getParent() ~= nil, "Nested declarative window root must already belong to its top-level asset")
    assert(host._window == nil and host.content == nil, "Declarative window host must defer its base view")
    local pauseMark = host._pauseMark
    local returnButton = host._returnButton
    ---@cast pauseMark Engine.Image
    ---@cast returnButton Engine.Button
    local windowFrame = self:getWindowFrame()
    local content = self:getContent()
    local repeated = host._repeated
    if repeated == nil then
        repeated = false
    end
    if host._windowSkin ~= nil then
        windowFrame:setWindowSkin(host._windowSkin, repeated)
    end
    host._window = windowFrame
    host.content = content
    host._visualRoot = root
    content:addChild(pauseMark)
    root:addChild(returnButton)
    host:_setUiController(self)
    return root
end

---@param host Source.Windows.Base.WindowBase
---@param root Engine.ControlBase
---@return Engine.ControlBase
function UiController:_attachWindowRoot(host, root, transitionTarget)
    assert(host._window == nil and host.content == nil, "Declarative window host must defer its base view")
    local pauseMark = host._pauseMark
    local returnButton = host._returnButton
    ---@cast pauseMark Engine.Image
    ---@cast returnButton Engine.Button
    local windowFrame = self:getWindowFrame()
    local content = self:getContent()
    local repeated = host._repeated
    if repeated == nil then
        repeated = false
    end
    if host._windowSkin ~= nil then
        windowFrame:setWindowSkin(host._windowSkin, repeated)
    end
    host:addChild(root)
    host._window = windowFrame
    host.content = content
    content:addChild(pauseMark)
    local chromeRoot = transitionTarget ~= nil and self:requireControl(transitionTarget) or root
    ---@cast chromeRoot Engine.Canvas
    chromeRoot:addChild(returnButton)
    host:_setUiController(self, transitionTarget)
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
