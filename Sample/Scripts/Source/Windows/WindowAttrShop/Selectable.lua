local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local _ITEM_ROW_HEIGHT = 32

---@class (partial) Source.Windows.WindowAttrShop.Selectable
local WindowAttrShopSelectable = {}

function WindowAttrShopSelectable:init(rect, owner)
    super(WindowAttrShopSelectable, self).init(rect, nil, rect.size.x - 64, _ITEM_ROW_HEIGHT, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._owner = owner
    self._abilityKeys = {}
    self._cellAvailable = {}
    owner._shopUI:attachSelectable(self, rect.size)
    self:setScrollBox(owner._shopUI:getScrollBox())
    self:setListView(owner._shopUI:getListView())
end

function WindowAttrShopSelectable:refresh(abilities, prices, moneyName, moneyAmount)
    self._owner._shopUI:refreshRows(abilities, prices, moneyName, moneyAmount)
end

function WindowAttrShopSelectable:getSelectedAbilityKey()
    if self.index == nil or self.index < 0 or self.index >= #self._abilityKeys then
        return nil
    end
    return self._abilityKeys[self.index + 1]
end

function WindowAttrShopSelectable:isCurrentAvailable()
    if self.index == nil or self.index < 0 or self.index >= #self._cellAvailable then
        return false
    end
    return self._cellAvailable[self.index + 1]
end

function WindowAttrShopSelectable:onTick(deltaTime)
    self._owner._shopUI:tick(deltaTime)
    super(WindowAttrShopSelectable, self).onTick(deltaTime)
end

function WindowAttrShopSelectable:onReturn()
    self._owner:closeByCancel()
end

return class(WindowAttrShopSelectable, WindowSelectable)
