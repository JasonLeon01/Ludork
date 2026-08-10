---@meta Mixins.Consumables.Bottle
--- @brief
---
---@class Mixins.Consumables.Bottle: Engine.Actor
---@field HP_plus integer
---@field getSE string
local Bottle = {}

---@param other Engine.Actor[]
function Bottle:onCollision(other) end

return Bottle
