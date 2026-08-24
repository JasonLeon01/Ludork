local Engine = require("Engine")
local UiController = require("Source.UI.UiController")

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
            if instance._bound == true and instance._disposed ~= true then
                instance:_refreshFromEvent(payload)
            end
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

function Ui.Define(assetKey, definition, baseClass)
    local updateEvent = Ui.GetEventName(assetKey)
    local channel = getChannel(updateEvent)
    local function unregister(instance)
        channel.instances[instance] = nil
    end
    definition.assetKey = assetKey
    definition.viewUpdateEvent = updateEvent
    definition.Publish = function (payload)
        Engine.publish(updateEvent, payload)
    end
    definition._registerUiInstance = function (instance)
        channel.instances[instance] = true
        instance:_setViewUpdateUnregister(unregister)
    end
    return class(definition, baseClass or UiController)
end

return Ui
