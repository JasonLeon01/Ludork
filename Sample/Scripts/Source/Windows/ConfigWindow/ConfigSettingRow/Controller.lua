local GameSystem = require("Source.System")
local ConfigRowControllerBase = require("Source.Windows.ConfigWindow.ConfigRow")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.ConfigWindow.ConfigSettingRow")

local ConfigSettingRowController = {}

function ConfigSettingRowController:init(ui, labelText, items, selectedIndex)
    self._items = items
    if selectedIndex == nil then
        selectedIndex = 0
    end
    self._selectedIndex = selectedIndex
    super(ConfigSettingRowController, self).init(ui, labelText)
end

function ConfigSettingRowController:bind()
    self.ui.controls["DropBox"]:setCanReceiveFocus(false)
    self.ui.controls["DropBox"]:setItems(self._items)
    self.ui.controls["DropBox"]:setSelectedIndex(self._selectedIndex)
    self.ui.controls["DropBox"]:setOpenSound(GameSystem.GetDecisionSE())
    self.ui.controls["DropBox"]:setCursorSound(GameSystem.GetCursorSE())
    self.ui.controls["DropBox"]:setSelectSound(GameSystem.GetDecisionSE())
    self.ui.controls["DropBox"]:setCancelSound(GameSystem.GetCancelSE())
    self.root:addConfirmCallback(self:bindCallback(ConfigSettingRowController.open))
end

function ConfigSettingRowController:open()
    self.ui.controls["DropBox"]:open()
end

function ConfigSettingRowController:refresh()
    self:setText("Label", self._labelText)
end

function ConfigSettingRowController:setItems(items)
    self._items = items
    self.ui.controls["DropBox"]:setItems(items)
    self:prepare()
end

function ConfigSettingRowController:setActive(active)
    super(ConfigSettingRowController, self).setActive(active)
    self.ui.controls["DropBox"]:setActive(active)
end

return Ui.Define(View, ConfigSettingRowController, ConfigRowControllerBase)
