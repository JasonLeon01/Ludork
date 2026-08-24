local ConfigRowControllerBase = require("Source.UI.Parts.ConfigWindow.ConfigRow")
local Ui = require("Source.UI.Ui")

local _ROW_HEIGHT = 32

local ConfigCheckBoxRowUI = {}

function ConfigCheckBoxRowUI:init(labelText, rowWidth, checkBoxSize, windowSkin, checked, onCheckedChanged)
    self._labelText = labelText
    self._checkBoxSize = checkBoxSize
    self._windowSkin = windowSkin
    if checked == nil then
        checked = false
    end
    self._checked = checked
    self._onCheckedChanged = onCheckedChanged
    super(ConfigCheckBoxRowUI, self).init(nil, rowWidth, _ROW_HEIGHT)
end

function ConfigCheckBoxRowUI:bind()
    self._label = self:requireControl("Label")
    self._checkBox = self:requireControl("CheckBox")
    self._checkBox:setCanReceiveFocus(false)
    self._checkBox:resize(sf.Vector2f.new(self._checkBoxSize, self._checkBoxSize))
    self._checkBox:setWindowSkin(self._windowSkin)
    self._checkBox:setChecked(self._checked)
    self._checkBox:setOnCheckedChanged(self._onCheckedChanged)
    self.root:addConfirmCallback(function (_, kwargs)
        self:_onConfirmToggle(kwargs)
    end)
end

function ConfigCheckBoxRowUI:refresh()
    self:setText("Label", self._labelText)
end

function ConfigCheckBoxRowUI:getCheckBox()
    return self._checkBox
end

function ConfigCheckBoxRowUI:setActive(active)
    super(ConfigCheckBoxRowUI, self).setActive(active)
    self._checkBox:setActive(active)
end

function ConfigCheckBoxRowUI:dispose()
    if self._checkBox ~= nil then
        self._checkBox:setOnCheckedChanged(nil)
    end
    self.root:addConfirmCallback(nil)
    self._onCheckedChanged = nil
    self._checkBox = nil
    super(ConfigCheckBoxRowUI, self).dispose()
end

---@param _kwargs table
function ConfigCheckBoxRowUI:_onConfirmToggle(_kwargs)
    self._checkBox:toggle()
end

return Ui.Define("Parts/ConfigWindow/ConfigCheckBoxRow", ConfigCheckBoxRowUI, ConfigRowControllerBase)
