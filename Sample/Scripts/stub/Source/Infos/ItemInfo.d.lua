---@meta Source.Infos.ItemInfo
--- @brief Item data + logic layer.
---
--- Defines item-related blueprint events (onUse, onGet).
--- Independent of Actor; can be used standalone in inventory/shop UI.
---
---@class Source.Infos.ItemInfo: Engine.InfoBase
---@field new fun(): Source.Infos.ItemInfo
local ItemInfo = {}

--- @brief Triggered when the item is used.
function ItemInfo:onUse() end

--- @brief Triggered when the item is gotten.
function ItemInfo:onGet() end

return ItemInfo
