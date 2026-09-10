---@meta Mixins.Consumables.Gem
---@brief
---
---@class (partial) Mixins.Consumables.Gem: Engine.Actor
---@field ATTR_key string
---@field plus     integer
---@field getSE    string
local Gem = {}

---@param other Engine.Actor[]
function Gem:onCollision(other) end

return Gem
