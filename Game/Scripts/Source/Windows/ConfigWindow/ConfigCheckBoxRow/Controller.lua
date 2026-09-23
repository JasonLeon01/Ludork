local ConfigRowControllerBase = require("Source.Windows.ConfigWindow.ConfigRow")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.ConfigWindow.ConfigCheckBoxRow")

local ConfigCheckBoxRowController = {}

function ConfigCheckBoxRowController:init(ui, labelText, checked, onCheckedChanged)
    if checked == nil then
        checked = false
    end
    self._checked = checked
    self._onCheckedChanged = onCheckedChanged
    super(ConfigCheckBoxRowController, self).init(ui, labelText)
end

function ConfigCheckBoxRowController:bind()
    self.ui.controls["CheckBox"]:setCanReceiveFocus(false)
    self.ui.controls["CheckBox"]:setChecked(self._checked)
    self.ui.controls["CheckBox"]:setOnCheckedChanged(self._onCheckedChanged)
    self.root:addConfirmCallback(function (_, kwargs)
        self:_onConfirmToggle(kwargs)
    end)
end

function ConfigCheckBoxRowController:refresh()
    self:setText("Label", self._labelText)
end

function ConfigCheckBoxRowController:setActive(active)
    super(ConfigCheckBoxRowController, self).setActive(active)
    self.ui.controls["CheckBox"]:setActive(active)
end

---@param _kwargs table
function ConfigCheckBoxRowController:_onConfirmToggle(_kwargs)
    self.ui.controls["CheckBox"]:toggle()
end

return Ui.Define(View, ConfigCheckBoxRowController, ConfigRowControllerBase)
