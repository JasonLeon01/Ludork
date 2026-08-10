---@meta Source.Pickup

local Pickup = {}

---@param actor           Engine.Actor
---@param other           Engine.Actor[]
---@param parentCollision fun(other: Engine.Actor[])
---@param applyPickup     fun(player: Source.Player.Player, inst: Source.GameInstance.GameInstance, scene: Source.Scenes.SceneMap.SceneMap)
function Pickup.handleCollision(actor, other, parentCollision, applyPickup) end

---@param actor           Engine.Actor
---@param other           Engine.Actor[]
---@param parentCollision fun(other: Engine.Actor[])
---@param applyPickup     fun(player: Source.Player.Player, inst: Source.GameInstance.GameInstance, scene: Source.Scenes.SceneMap.SceneMap)
function Pickup.handleInventoryCollision(actor, other, parentCollision, applyPickup) end

return Pickup
