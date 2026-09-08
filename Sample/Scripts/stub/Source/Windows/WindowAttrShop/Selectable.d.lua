---@meta Source.Windows.WindowAttrShop.Selectable

---@class Source.Windows.WindowAttrShop.Selectable: Source.Windows.Base.WindowSelectable
---@field _owner    Source.Windows.WindowAttrShop
---@field _listView Engine.ListView
---@field new       fun(rect: sf.IntRect, owner: Source.Windows.WindowAttrShop, ui: Source.UI.WindowAttrShop): Source.Windows.WindowAttrShop.Selectable
---@field _ui       Source.UI.WindowAttrShop
local WindowAttrShopSelectable = {}

---@brief Construct the attribute shop selection window.
---
--- - @param rect Window rectangle.
--- - @param owner Attribute shop coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowAttrShop
---@param ui    Source.UI.WindowAttrShop
function WindowAttrShopSelectable:init(rect, owner, ui) end

---@brief Rebuild the ability rows and leave command.
function WindowAttrShopSelectable:refresh() end

---@brief Get the selected player attribute name.
---
--- - @return Attribute name, or nil when Leave is selected.
---@return string | nil
function WindowAttrShopSelectable:getSelectedAbilityKey() end

---@brief Return whether the selected row can be confirmed.
---@return boolean
function WindowAttrShopSelectable:isCurrentAvailable() end

---@param deltaTime number
function WindowAttrShopSelectable:onTick(deltaTime) end

function WindowAttrShopSelectable:onReturn() end

return WindowAttrShopSelectable
