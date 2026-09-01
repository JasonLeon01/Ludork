---@meta Source.Item
---@class Source.Item: Engine.Actor
---@field ID         string
---@field attributes Source.Configs.GeneralDataTypes.ItemAttributeSet
---@field count      integer
---@field getSE      string
---@field new        fun(texture?: sf.Texture, rect?: sf.IntRect, tag?: string): Source.Item
local Item = {}

---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Item:init(texture, rect, tag) end

---@param other Engine.Actor[]
function Item:onCollision(other) end

return Item
