---@meta Source.MapActors.Equip
---@class Source.MapActors.Equip: Source.MapActors.ConditionalActor
---@field ID         string
---@field attributes Source.Configs.GeneralDataTypes.EquipAttributeSet
---@field getSE      string
---@field new        fun(texture?: sf.Texture, rect?: sf.IntRect, tag?: string): Source.MapActors.Equip
local Equip = {}

---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Equip:init(texture, rect, tag) end

---@param other Engine.Actor[]
function Equip:onCollision(other) end

return Equip
