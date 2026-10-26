---@meta Source.Utils.GameInstance.Shops

---@class Source.Utils.GameInstance.Shops.Module
local Shops = {}

---@param shops   Source.GameInstance.ShopRecord[]
---@param mapPath string
---@param mapTag  string
---@return boolean added
function Shops.Append(shops, mapPath, mapTag) end

---@param shops   Source.GameInstance.ShopRecord[]
---@param mapPath string
---@param mapTag  string
---@return boolean removed
function Shops.Remove(shops, mapPath, mapTag) end

---@param shops Source.GameInstance.ShopRecord[] | nil
---@return Source.GameInstance.ShopRecord[]
function Shops.Decode(shops) end

return Shops
