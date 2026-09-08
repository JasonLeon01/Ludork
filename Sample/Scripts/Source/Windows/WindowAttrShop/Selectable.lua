local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local _ITEM_ROW_HEIGHT = 32

---@class (partial) Source.Windows.WindowAttrShop.Selectable
local WindowAttrShopSelectable = {}

function WindowAttrShopSelectable:init(rect, owner, ui)
    super(WindowAttrShopSelectable, self).init(rect, nil, rect.size.x - 64, _ITEM_ROW_HEIGHT, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._owner = owner
    self._ui = ui
    ui:attachSelectable(self, rect.size)
    self:setScrollBox(ui:getScrollBox())
    self:setListView(ui:getListView())
end

function WindowAttrShopSelectable:refresh()
    self._ui:refreshRows()
end

function WindowAttrShopSelectable:getSelectedAbilityKey()
    return self._ui:getSelectedAbilityKey()
end

function WindowAttrShopSelectable:isCurrentAvailable()
    return self._ui:isCurrentAvailable()
end

function WindowAttrShopSelectable:onTick(deltaTime)
    self._ui:tick(deltaTime)
    super(WindowAttrShopSelectable, self).onTick(deltaTime)
end

function WindowAttrShopSelectable:onReturn()
    self._owner:closeByCancel()
end

return class(WindowAttrShopSelectable, WindowSelectable)
