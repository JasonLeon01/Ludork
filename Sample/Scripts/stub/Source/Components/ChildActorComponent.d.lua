---@meta Source.Components.ChildActorComponent
---@class Source.Components.ChildActorComponent: Engine.Component
---@field className        string
---@field relativePosition sf.Vector2f
---@field _childActor      Engine.Actor | nil
local ChildActorComponent = {}

---@return Source.Components.ChildActorComponent
function ChildActorComponent.new(...) end

---@param values { className?: string, relativePosition?: sf.Vector2f } | nil
function ChildActorComponent:init(values) end

---
--- @brief Spawn and attach the configured child actor.
---
--- - @param owner Actor that owns this component.
--- - @return The spawned child actor, or an empty list.
---
---@param owner Engine.Actor
---@return Engine.Actor[]
function ChildActorComponent:onAttach(owner) end

return ChildActorComponent
