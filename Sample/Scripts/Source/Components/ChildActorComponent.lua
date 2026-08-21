local Engine = require("Engine")

local Component = Engine.Component

local ChildActorComponent = {}

ChildActorComponent.className = ""
ChildActorComponent.relativePosition = sf.Vector2f.new(0.0, 0.0)

function ChildActorComponent:init(values)
    values = values or {}
    self.className = values.className == nil and ChildActorComponent.className or values.className
    local relativePosition = values.relativePosition == nil and ChildActorComponent.relativePosition
        or values.relativePosition
    self.relativePosition = copy(relativePosition)
    ---@type Engine.Actor | nil
    self._childActor = nil
end

function ChildActorComponent:onAttach(owner)
    local className = self.className:match("^%s*(.-)%s*$")
    if not bool(className) then
        return {}
    end
    ---@cast className string

    local actorMap = owner:getMap()
    if actorMap == nil then
        return {}
    end

    if self._childActor ~= nil and not self._childActor:isDestroyed() then
        return {}
    end

    local Data = require("Source.Data")

    local childActor = Data.genActorFromClassName(className, ChildActorComponent._makeChildTag(owner))
    if childActor == nil then
        return {}
    end

    owner:addChild(childActor)
    childActor:setRelativePosition(self.relativePosition)
    local layer = actorMap:getActorLayer(owner)
    if layer == nil then
        layer = "default"
    end
    actorMap:spawnActor(childActor, layer, false)
    self._childActor = childActor
    return { childActor }
end

---@param owner Engine.Actor
---@return string
function ChildActorComponent._makeChildTag(owner)
    local parentTag = owner:getMapTag()
    if parentTag == nil then
        parentTag = ""
    end
    return tostring(parentTag) .. "_child"
end

return class(ChildActorComponent, Component)
