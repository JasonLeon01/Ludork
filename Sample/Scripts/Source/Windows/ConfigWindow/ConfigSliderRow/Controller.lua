local ConfigRowControllerBase = require("Source.Windows.ConfigWindow.ConfigRow")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.ConfigWindow.ConfigSliderRow")

local ConfigSliderRowController = {}

function ConfigSliderRowController:init(ui, labelText, value, onValueChanged)
    if value == nil then
        value = 0
    end
    self._value = value
    self._onValueChanged = onValueChanged
    super(ConfigSliderRowController, self).init(ui, labelText)
end

function ConfigSliderRowController:bind()
    self.ui.controls["Slider"]:setCanReceiveFocus(false)
    if PLATFORM == "ios" or PLATFORM == "android" or PLATFORM == "ohos" then
        local sliderSize = self.ui.controls["Slider"]:getSize()
        local rowHeight = self.root:getSize().y
        self.ui.controls["Slider"]:setTouchHitBounds(
            sf.FloatRect.new(
                sf.Vector2f.new(0.0, (sliderSize.y - rowHeight) / 2.0), sf.Vector2f.new(sliderSize.x, rowHeight)
            )
        )
    end
    self.ui.controls["Slider"]:setValue(self._value)
    self.ui.controls["Slider"]:setOnValueChanged(self:bindCallback(ConfigSliderRowController._onSliderValueChanged))
end

function ConfigSliderRowController:refresh()
    self:setText("Label", self._labelText)
    self:setText("ValueText", tostring(self.ui.controls["Slider"]:getValue()))
end

function ConfigSliderRowController:setActive(active)
    super(ConfigSliderRowController, self).setActive(active)
    self.ui.controls["Slider"]:setActive(active)
end

function ConfigSliderRowController:adjust(delta)
    self.ui.controls["Slider"]:adjust(delta)
end

---@param value integer
function ConfigSliderRowController:_onSliderValueChanged(value)
    self:_refreshValueText()
    if self._onValueChanged ~= nil then
        self._onValueChanged(value)
    end
end

function ConfigSliderRowController:_refreshValueText()
    self:setText("ValueText", tostring(self.ui.controls["Slider"]:getValue()))
    self.view:reflow()
    self.root:render()
end

return Ui.Define(View, ConfigSliderRowController, ConfigRowControllerBase)
