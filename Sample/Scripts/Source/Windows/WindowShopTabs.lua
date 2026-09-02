local WindowShopTabsUI = require("Source.UI.Parts.WindowShop.WindowShopTabs")
local WindowBase = require("Source.Windows.Base.WindowBase")

---@class Source.Windows.WindowShopTabs
local WindowShopTabs = {}

function WindowShopTabs:init(rect, owner, instance)
    super(WindowShopTabs, self).init(rect, nil, nil, true)
    self:setCanReceiveFocus(false)
    self._owner = owner
    self._ui = WindowShopTabsUI.new(self, rect.size, instance)
    self._ui:attach(instance ~= nil)
    self._tabView = self._ui:getTabView()
end

function WindowShopTabs:getTabView()
    return self._tabView
end

function WindowShopTabs:onSelectedIndexChanged(index)
    self._owner:onTabSelected(index)
end

function WindowShopTabs:handleNavigationInput()
    return self._tabView:handleNavigationInput()
end

function WindowShopTabs:refresh()
    self._ui:refresh()
end

function WindowShopTabs:dispose()
    self._ui:dispose()
    self._ui = nil
    self._tabView = nil
    self._owner = nil
end

return class(WindowShopTabs, WindowBase)
