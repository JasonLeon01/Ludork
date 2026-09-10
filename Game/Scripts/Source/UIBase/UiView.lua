local Engine = require("Engine")
local UiCollection = require("Source.UIBase.UiCollection")
local WindowTransition = require("Source.UIBase.WindowTransition")

local function clearControlCallbacks(control)
    if Class.isInstance(control, Engine.FunctionalBase) then
        ---@cast control Engine.FunctionalBase
        control:addConfirmCallback(nil)
        control:addCancelCallback(nil)
        control:addClickCallback(nil)
        control:addMouseButtonDownCallback(nil)
        control:addHoverCallback(nil)
        control:addUnHoverCallback(nil)
        control:addMouseMovedCallback(nil)
        control:addMouseWheelScrolledCallback(nil)
        control:addKeyDownCallback(nil)
        control:addKeyUpCallback(nil)
    end
    if Class.isInstance(control, Engine.CheckBox) then
        ---@cast control Engine.CheckBox
        control:setOnCheckedChanged(nil)
    elseif Class.isInstance(control, Engine.DropBox) then
        ---@cast control Engine.DropBox
        control:setOnSelectedIndexChanged(nil)
        control:setOnSelectionConfirmed(nil)
        control:setOnExpandedChanged(nil)
    elseif Class.isInstance(control, Engine.Slider) then
        ---@cast control Engine.Slider
        control:setOnValueChanged(nil)
    elseif Class.isInstance(control, Engine.TabView) then
        ---@cast control Engine.TabView
        control:setOnSelectedIndexChanged(nil)
    elseif Class.isInstance(control, Engine.TextBox) then
        ---@cast control Engine.TextBox
        control:setOnTextChanged(nil)
        control:setOnEditingChanged(nil)
    end
end

---@class Source.UIBase.UiView
local UiView = {}

UiView.assetKey = ""
UiView.designSize = sf.Vector2f.new(0, 0)

function UiView:init(instance)
    if instance == nil then
        assert(bool(self.assetKey), "UiView asset key is required")
        instance = assert(Engine.instantiate(self.assetKey))
    end
    self.instance = instance
    self.root = instance:getRoot()
    self.controls = {}
    self.assets = {}
    self._owned = {}
    self._collections = {}
    self._windowBinding = nil
    self._controller = nil
    self._controllerDispose = nil
    self._uiManager = nil
    self._mounted = false
    self._disposed = false
    self._logicalSize = nil
    self._animationBindings = {}
    self._animationGenerations = {}
end

function UiView:own(child)
    self._owned[child] = child.dispose
    return child
end

function UiView:bindController(controller)
    assert(self._controller == nil, "UI View already has a Controller")
    self._controller = controller
    self._controllerDispose = controller.dispose
end

function UiView:bindWindow(binding)
    self._windowBinding = binding
end

function UiView:syncWindowHosts()
    if self._windowBinding ~= nil then
        self._windowBinding:syncLayout()
    end
    for _, child in pairs(self.assets) do
        child:syncWindowHosts()
    end
end

function UiView:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    self.instance:reflow(self._logicalSize)
    self:syncWindowHosts()
    return self.root
end

function UiView:attachTo(parent, logicalSize)
    self:prepare(logicalSize)
    return self:attachPreparedTo(parent)
end

function UiView:attachPreparedTo(parent)
    if self.root:getParent() ~= parent then
        self:detachControl(self.root)
        parent:addChild(self.root)
    end
    return self.root
end

function UiView:getWindowFrame()
    local frame = assert(self.controls["WindowFrame"], "UI asset has no WindowFrame control")
    assert(Class.isInstance(frame, Engine.Window), "WindowFrame must be an Engine.Window")
    ---@cast frame Engine.Window
    return frame
end

function UiView:getContent()
    local content = assert(self.controls["Content"], "UI asset has no Content control")
    assert(Class.isInstance(content, Engine.Canvas), "Content must be an Engine.Canvas")
    ---@cast content Engine.Canvas
    return content
end

function UiView:attachWindowView(host, logicalSize, transitionTarget)
    self:prepare(logicalSize)
    return self:attachPreparedWindow(host, self:getWindowFrame(), self:getContent(), false, transitionTarget)
end

function UiView:attachNestedWindowView(host, logicalSize)
    self:prepare(logicalSize)
    return self:attachPreparedWindow(host, self:getWindowFrame(), self:getContent(), true)
end

function UiView:attachPreparedWindow(host, frame, content, nested, transitionTarget)
    local chromeRoot = transitionTarget ~= nil and assert(self.instance:requireControl(transitionTarget)) or self.root
    ---@cast chromeRoot Engine.Canvas
    host:attachPreparedView(self, {
        root = self.root,
        windowFrame = frame,
        content = content,
        chromeRoot = chromeRoot,
        transitionTarget = transitionTarget,
        nested = nested
    })
    return self.root
end

function UiView:createTransition(host, target)
    return WindowTransition.new(host, self, target)
end

function UiView:createCollection(container, controllerClass)
    assert(self._collections[container] == nil, "UI container already has a dynamic collection")
    local collection = self:own(UiCollection.new(container, controllerClass))
    self._collections[container] = collection
    return collection
end

function UiView:hasAnimation(name, target)
    return self.instance:hasAnimation(name, target)
end

function UiView:playAnimation(name, target, onFinished)
    local key = target or ""
    local generation = (self._animationGenerations[key] or 0) + 1
    self._animationGenerations[key] = generation
    self._animationBindings[key] = { name = name, target = target }
    local started = self.instance:playAnimation(name, target, function ()
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

function UiView:stopAnimation(name, target)
    local key = target or ""
    if self._animationBindings[key] ~= nil and self._animationBindings[key].name == name then
        self._animationGenerations[key] = (self._animationGenerations[key] or 0) + 1
        self._animationBindings[key] = nil
    end
    self.instance:stopAnimation(name, target)
end

---@diagnostic disable-next-line: unused
function UiView:detachControl(control)
    local parent = control:getParent()
    if parent ~= nil then
        ---@cast parent Engine.Canvas | Engine.ListView
        parent:removeChild(control)
    end
end

function UiView:mount(uiManager, logicalSize)
    self:prepare(logicalSize)
    self:mountPrepared(uiManager)
end

function UiView:mountPrepared(uiManager)
    if not self._mounted then
        uiManager:loadUI(self.root)
        self._uiManager = uiManager
        self._mounted = true
    end
end

function UiView:unmount()
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

function UiView:dispose()
    if self._disposed then
        return
    end
    self._disposed = true
    if self._controllerDispose ~= nil then
        self._controllerDispose(self._controller)
    end
    self:unmount()
    for key, binding in pairs(self._animationBindings) do
        self._animationGenerations[key] = (self._animationGenerations[key] or 0) + 1
        self.instance:stopAnimation(binding.name, binding.target)
    end
    for _, control in pairs(self.controls) do
        clearControlCallbacks(control)
    end
    for child, dispose in pairs(self._owned) do
        dispose(child)
    end
    self:detachControl(self.root)
end

return class(UiView)
