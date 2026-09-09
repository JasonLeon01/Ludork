local Engine = require("Engine")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowShop.WindowShopTabs")
local WindowBase = require("Source.Windows.Base.WindowBase")

---@class Source.Windows.WindowShopTabs.Controller
local Controller = {}

Controller.windowOptions = { focusable = false, hidden = true }

function Controller:init(owner)
    self._owner = owner
end

function Controller:onSelectedIndexChanged(index)
    self._owner:onTabSelected(index)
end

function Controller:handleNavigationInput()
    return self.ui.controls["Tabs"]:handleNavigationInput()
end

function Controller:refresh()
    self.ui.controls["Tabs"]:setItems(LocaleCore.ApplyListLocaleFormat({ "SHOP_BUY", "SHOP_SELL" }))
end

function Controller:dispose()
    self._owner = nil
    super(Controller, self).dispose()
end

function Controller:bind()
    self.ui.controls["Tabs"]:setKeyHint(
        Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.Q, Joystick = Engine.JoystickButton.getLB() }),
        Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.E, Joystick = Engine.JoystickButton.getRB() })
    )
    self.ui.controls["Tabs"]:setCursorSound(GameSystem.GetCursorSE())
    self.ui.controls["Tabs"]:setOnSelectedIndexChanged(self:bindCallback(Controller.onSelectedIndexChanged))
end

return Ui.DefineWindow(View, Controller, WindowBase)
