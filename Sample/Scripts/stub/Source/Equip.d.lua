---@meta Source.Equip
---@class Source.Equip: Engine.Actor
---@field ID         string
---@field attributes Source.Configs.GeneralDataTypes.EquipAttributeSet
---@field getSE      string
---@field new        fun(texture?: sf.Texture, rect?: sf.IntRect, tag?: string): Source.Equip
local Equip = {}

---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Equip:init(texture, rect, tag) end

---@param other Engine.Actor[]
function Equip:onCollision(other) end

return Equip
