local Engine = require("Engine")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local WindowShopTabsUI = {}

function WindowShopTabsUI:init(model, size, instance)
    self._size = size
    super(WindowShopTabsUI, self).init(model, instance)
end

function WindowShopTabsUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._tabView = self:requireControl("Tabs")
    self._tabView:setKeyHint(
        Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.Q, Joystick = Engine.JoystickButton.getLB() }),
        Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.E, Joystick = Engine.JoystickButton.getRB() })
    )
    self._tabView:setCursorSound(GameSystem.GetCursorSE())
    self._tabView:setOnSelectedIndexChanged(function (index)
        self.model:onSelectedIndexChanged(index)
    end)
end

function WindowShopTabsUI:refresh()
    self._tabView:setItems({ LOC("SHOP_BUY"), LOC("SHOP_SELL") })
end

function WindowShopTabsUI:prepare()
    return super(WindowShopTabsUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowShopTabsUI:attach(nested)
    if nested == true then
        self:attachNestedWindowView(self.model)
    else
        self:attachWindowView(self.model)
    end
end

function WindowShopTabsUI:getWindowFrame()
    return self._windowFrame
end

function WindowShopTabsUI:getContent()
    return self._content
end

function WindowShopTabsUI:getTabView()
    return self._tabView
end

function WindowShopTabsUI:dispose()
    if self._tabView ~= nil then
        self._tabView:setOnSelectedIndexChanged(nil)
        self._tabView = nil
    end
    super(WindowShopTabsUI, self).dispose()
end

return Ui.Define("Parts/WindowShop/WindowShopTabs", WindowShopTabsUI)
