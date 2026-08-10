---@meta Source.Item
--- @brief Scene item entity.
---
--- Bridges Actor (rendering/collision/movement) and ItemInfo (item data + event logic)
--- via multiple inheritance.
---
local Item = {}

--- @brief Construct an item with actor rendering and item info.
---
--- - @param texture Optional sf.Texture for the actor sprite.
--- - @param rect Optional sf.IntRect texture rectangle.
--- - @param tag Optional actor tag.
---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Item:init(texture, rect, tag) end

---@param other Engine.Actor[]
function Item:onCollision(other) end

return Item
