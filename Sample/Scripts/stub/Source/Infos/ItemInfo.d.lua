---@meta Source.Infos.ItemInfo
--- @brief Item data + logic layer.
---
--- Defines item-related blueprint events (onUse, onGet).
--- Independent of Actor; can be used standalone in inventory/shop UI.
---
local ItemInfo = {}

---@return any
function ItemInfo.new(...) end

--- @brief Triggered when the item is used.
function ItemInfo:onUse() end

--- @brief Triggered when the item is gotten.
function ItemInfo:onGet() end

return ItemInfo
