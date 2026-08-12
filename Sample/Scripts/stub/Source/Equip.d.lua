---@meta Source.Equip
--- @brief Scene equip entity.
---
--- Bridges Actor (rendering/collision/movement) and EquipInfo (equip data + event logic)
--- via multiple inheritance.
---
---@class Source.Equip: Engine.Actor, Source.Infos.EquipInfo
---@field getSE string
---@field new fun(texture?: sf.Texture, rect?: sf.IntRect, tag?: string): Source.Equip
local Equip = {}

--- @brief Construct an equip with actor rendering and equip info.
---
--- - @param texture Optional sf.Texture for the actor sprite.
--- - @param rect Optional sf.IntRect texture rectangle.
--- - @param tag Optional actor tag.
---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Equip:init(texture, rect, tag) end

---@param other Engine.Actor[]
function Equip:onCollision(other) end

return Equip
