---@meta Source.Windows.WindowAttrShop.Selectable

---@class Source.Windows.WindowAttrShop.Selectable: Source.Windows.Base.WindowSelectable
---@field _owner         Source.Windows.WindowAttrShop
---@field _abilityKeys   string[]
---@field _cellAvailable boolean[]
---@field _listView      Engine.ListView
---@field new            fun(rect: sf.IntRect, owner: Source.Windows.WindowAttrShop): Source.Windows.WindowAttrShop.Selectable
local WindowAttrShopSelectable = {}

---@brief Construct the attribute shop selection window.
---
--- - @param rect Window rectangle.
--- - @param owner Attribute shop coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowAttrShop
function WindowAttrShopSelectable:init(rect, owner) end

---@brief Rebuild the ability rows and leave command.
---
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param prices Purchase prices ordered to match abilities.
--- - @param moneyName Player info component attribute used as currency.
--- - @param moneyAmount Current amount of the selected currency.
---@param abilities   table
---@param prices      table
---@param moneyName   string
---@param moneyAmount integer
function WindowAttrShopSelectable:refresh(abilities, prices, moneyName, moneyAmount) end

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
