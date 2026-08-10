local ConfigRowControllerBase = require("Source.UI.Parts.ConfigWindow.ConfigRow")
local Ui = require("Source.UI.Ui")

local _ROW_HEIGHT = 32
local _VALUE_PAD_X = 16

local ConfigSliderRowUI = {}

function ConfigSliderRowUI:init(labelText, rowWidth, sliderWidth, value, onValueChanged)
    self._labelText = labelText
    self._sliderWidth = sliderWidth
    if value == nil then
        value = 0
    end
    self._value = value
    self._onValueChanged = onValueChanged
    super(ConfigSliderRowUI, self).init(nil, rowWidth, _ROW_HEIGHT)
end

function ConfigSliderRowUI:bind()
    self._label = self:requireControl("Label")
    self._valueText = self:requireControl("ValueText")
    self._slider = self:requireControl("Slider")
    self._slider:setCanReceiveFocus(false)
    self._slider:resize(sf.Vector2f.new(self._sliderWidth, 8.0))
    self._slider:setRange(0, 100)
    self._slider:setValue(self._value)
    self._slider:setOnValueChanged(function (newValue)
        self:_onSliderValueChanged(newValue)
    end)
end

function ConfigSliderRowUI:refresh()
    self:setText("Label", self._labelText)
    self:setText("ValueText", tostring(self._slider:getValue()))
end

function ConfigSliderRowUI:prepare()
    local root = super(ConfigSliderRowUI, self).prepare()
    self:_applyValueTextPosition()
    root:render()
    return root
end

function ConfigSliderRowUI:getSlider()
    return self._slider
end

function ConfigSliderRowUI:setActive(active)
    super(ConfigSliderRowUI, self).setActive(active)
    self._slider:setActive(active)
end

function ConfigSliderRowUI:adjust(delta)
    self._slider:adjust(delta)
end

function ConfigSliderRowUI:dispose()
    if self._slider ~= nil then
        self._slider:setOnValueChanged(nil)
    end
    self._onValueChanged = nil
    self._slider = nil
    super(ConfigSliderRowUI, self).dispose()
end

---@param value integer
function ConfigSliderRowUI:_onSliderValueChanged(value)
    self:_refreshValueText()
    if self._onValueChanged ~= nil then
        self._onValueChanged(value)
    end
end

function ConfigSliderRowUI:_refreshValueText()
    self:setText("ValueText", tostring(self._slider:getValue()))
    self.view:reflow(sf.Vector2u.new(self._rowWidth, _ROW_HEIGHT))
    self:_applyValueTextPosition()
    self.root:render()
end

function ConfigSliderRowUI:_applyValueTextPosition()
    local bounds = self._valueText:getLocalBounds()
    local sliderPosition = self._slider:getPosition()
    local sliderSize = self._slider:getSize()
    local valueX = sliderPosition.x + sliderSize.x + _VALUE_PAD_X - bounds.position.x
    local valueY = (_ROW_HEIGHT - bounds.size.y) / 2.0
    self._valueText:setPosition(sf.Vector2f.new(valueX, valueY))
    self._valueText:setOrigin(sf.Vector2f.new(0.0, 0.0))
end

return Ui.define("Parts/ConfigWindow/ConfigSliderRow", ConfigSliderRowUI, ConfigRowControllerBase)
