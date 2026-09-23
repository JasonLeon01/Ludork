local Engine = require("Engine")
local UiController = require("Internal.UIBase.UiController")

local Ui = {}

local _EVENT_PREFIX = "UI:"
local _channels = {}

local function getChannel(eventName)
    local channel = _channels[eventName]
    if channel ~= nil then
        return channel
    end
    channel = {
        instances = setmetatable({}, {
            __mode = "k"
        })
    }
    channel.token = Engine.subscribe(eventName, function (payload)
        for instance in pairs(channel.instances) do
            instance:refreshFromEvent(payload)
        end
    end)
    _channels[eventName] = channel
    return channel
end

function Ui.GetEventName(assetKey)
    return _EVENT_PREFIX .. assetKey
end

function Ui.Publish(assetKey, payload)
    Engine.publish(Ui.GetEventName(assetKey), payload)
end

function Ui.DefineWindow(viewClass, definition, nativeBase)
    local UiWindow = require("Internal.UIBase.UiWindow")

    nativeBase = nativeBase or Engine.Canvas
    local options = definition.windowOptions or {}
    local windowDefinition = {}
    ---@type Class.ClassType<Internal.UIBase.Ui.Window>
    local windowClass
    local reuseView = {}
    for name, value in pairs(definition) do
        if Class.isInstance(value, "function") and name ~= "init" and name ~= "dispose" and name ~= "ready" then
            if not name:match("^[A-Z]") then
                windowDefinition[name] = function (window, ...)
                    if not window._controllerReady then
                        return super(windowClass, window)[name](...)
                    end
                    return window._controller[name](window._controller, ...)
                end
            else
                windowDefinition[name] = value
            end
        elseif name:match("^[A-Z][A-Z0-9_]*$") then
            windowDefinition[name] = value
        end
    end
    local init = definition.init
    definition.init = function (controller, window, ui, nested, ...)
        controller.host = window
        window._controller = controller
        window._controllerDispose = controller.dispose
        window._controllerReady = false
        window._windowBinding = UiWindow.new(window, ui, nativeBase, options, nested)
        window._windowBindingDispose = window._windowBinding.dispose
        UiController.init(controller, nil, ui)
        window.ui = controller.ui
        controller._hostDispose = window.dispose
        window._windowBinding:attach()
        controller._transition = window._windowBinding.transition
        window._controllerReady = true
        if init ~= nil then
            init(controller, ...)
        end
        controller:prepare()
        controller:ready()
        ui:syncWindowHosts()
        if options.hidden then
            controller._transition:hideImmediate()
        end
    end
    local controllerClass = Ui.Define(viewClass, definition)
    windowDefinition.init = function (window, ...)
        if select(1, ...) == reuseView then
            local ui = select(2, ...)
            assert(Class.isInstance(ui, viewClass), "Child window requires its matching generated View")
            controllerClass.new(window, ui, true, select(3, ...))
        else
            controllerClass.new(window, viewClass.new(), false, ...)
        end
    end
    windowDefinition.FromView = function (ui, ...)
        return windowClass.new(reuseView, ui, ...)
    end
    windowDefinition.mount = function (window, manager)
        return window._windowBinding:mount(manager)
    end
    windowDefinition.unmount = function (window)
        window._windowBinding:unmount()
    end
    windowDefinition.setPosition = function (window, position)
        if window._windowBinding ~= nil then
            window._windowBinding:setPosition(position)
        else
            Engine.ControlBase.setPosition(window, position)
        end
    end
    local update = windowDefinition.update
    windowDefinition.update = function (window, deltaTime)
        window._windowBinding:syncLayout()
        if update ~= nil then
            update(window, deltaTime)
        else
            super(windowClass, window).update(deltaTime)
        end
    end
    windowDefinition.dispose = function (window)
        if window._controllerDispose ~= nil then
            window._controllerDispose(window._controller)
        end
        if window._windowBindingDispose ~= nil then
            window._windowBindingDispose(window._windowBinding)
        end
    end
    windowDefinition.Publish = controllerClass.Publish
    local definedClass = class(windowDefinition, nativeBase)
    ---@cast definedClass Class.ClassType<Internal.UIBase.Ui.Window>
    windowClass = definedClass
    return windowClass
end

function Ui.Define(viewClass, definition, baseClass)
    local assetKey = viewClass.assetKey
    definition.viewClass = viewClass
    local updateEvent = Ui.GetEventName(assetKey)
    definition.assetKey = assetKey
    definition.viewUpdateEvent = updateEvent
    definition.Publish = function (payload)
        Engine.publish(updateEvent, payload)
    end
    definition.registerUiInstance = function (instance)
        local channel = getChannel(updateEvent)
        channel.instances[instance] = true
        instance:setViewUpdateUnregister(function (registeredInstance)
            channel.instances[registeredInstance] = nil
        end)
    end
    return class(definition, baseClass or UiController)
end

return Ui
