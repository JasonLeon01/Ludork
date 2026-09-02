local Engine = require("Engine")
local EventKeys = require("Source.Configs.EventKeys")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local WindowSaveTabsUI = {}

WindowSaveTabsUI.refreshEvents = { EventKeys.LocaleChanged }

local function setTabItems(tabView, model)
    tabView:setItems({ LOC("MENU_LOAD"), LOC("MENU_SAVE") }, function (index)
        model:onSelectedIndexChanged(index)
    end)
end

function WindowSaveTabsUI:init(model, size, instance)
    self._size = size
    super(WindowSaveTabsUI, self).init(model, instance)
end

function WindowSaveTabsUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._tabView = self:requireControl("Tabs")
    self._tabView:setKeyHint({
        Keyboard = sf.Keyboard.Key.Q,
        Joystick = Engine.JoystickButton.getLB()
    },
        {
            Keyboard = sf.Keyboard.Key.E,
            Joystick = Engine.JoystickButton.getRB()
        })
    self._tabView:setCursorSound(GameSystem.GetCursorSE())
    setTabItems(self._tabView, self.model)
end

function WindowSaveTabsUI:refresh()
    setTabItems(self._tabView, self.model)
end

function WindowSaveTabsUI:prepare()
    return super(WindowSaveTabsUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowSaveTabsUI:attach(nested)
    if nested == true then
        self:attachNestedWindowView(self.model)
    else
        self:attachWindowView(self.model)
    end
end

function WindowSaveTabsUI:getWindowFrame()
    return self._windowFrame
end

function WindowSaveTabsUI:getContent()
    return self._content
end

function WindowSaveTabsUI:getTabView()
    return self._tabView
end

function WindowSaveTabsUI:dispose()
    if self._tabView ~= nil then
        self._tabView:setItems(self._tabView:getItems(), nil)
        self._tabView = nil
    end
    super(WindowSaveTabsUI, self).dispose()
end

return Ui.Define("Parts/WindowSaveLoad/WindowSaveTabs", WindowSaveTabsUI)
