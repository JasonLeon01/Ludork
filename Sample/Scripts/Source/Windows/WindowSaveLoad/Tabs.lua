local WindowSaveTabsUI = require("Source.UI.Parts.WindowSaveLoad.WindowSaveTabs")
local WindowBase = require("Source.Windows.Base.WindowBase")

local WindowSaveTabs = {}

function WindowSaveTabs:init(rect, owner)
    super(WindowSaveTabs, self).init(rect, nil, nil, true)
    self:setCanReceiveFocus(false)
    self._owner = owner
    self._ui = WindowSaveTabsUI.new(self, rect.size)
    self._ui:attach()
    self._tabView = self._ui:getTabView()
end

function WindowSaveTabs:getTabView()
    return self._tabView
end

function WindowSaveTabs:onSelectedIndexChanged(index)
    self._owner:onTabSelected(index)
end

function WindowSaveTabs:handleNavigationInput()
    return self._tabView:handleNavigationInput()
end

function WindowSaveTabs:dispose()
    self._ui:dispose()
    self._ui = nil
    self._tabView = nil
    self._owner = nil
end

return class(WindowSaveTabs, WindowBase)
