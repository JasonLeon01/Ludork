local Data = require("Source.Data")
local EventKeys = require("Source.Configs.EventKeys")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")
local UiControlFactory = require("Source.UI.UiControlFactory")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local WindowSaveSlotUI = {}

WindowSaveSlotUI.refreshEvents = { EventKeys.LocaleChanged }

function WindowSaveSlotUI:init(model, size, maxSlots)
    self._size = size
    self._maxSlots = maxSlots
    self._slotItems = {}
    super(WindowSaveSlotUI, self).init(model, nil)
end

function WindowSaveSlotUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._listView = self:requireControl("SlotList")
    self._slotItems = {}
    for slotIndex = 0, self._maxSlots - 1 do
        local slot = slotIndex
        local root = UiControlFactory.createFunctionalPlainText(Data.getPlainTextConfig("UI/Default"))
        root:addConfirmCallback(function (_obj, _kwargs)
            self.model._owner:onSlotConfirm(slot)
        end)
        self.model:_applyItem(root)
        self._listView:addChild(root)
        self._slotItems[#self._slotItems + 1] = root
    end
end

function WindowSaveSlotUI:refresh()
    for slotIndex, root in ipairs(self._slotItems) do
        root:setString(LOC("SAVEFILE"):pformat(slotIndex))
    end
end

function WindowSaveSlotUI:prepare()
    return super(WindowSaveSlotUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowSaveSlotUI:attach()
    self:attachWindowView(self.model)
end

function WindowSaveSlotUI:getWindowFrame()
    return self._windowFrame
end

function WindowSaveSlotUI:getContent()
    return self._content
end

function WindowSaveSlotUI:getListView()
    return self._listView
end

return Ui.define("Parts/WindowSaveLoad/WindowSaveSlot", WindowSaveSlotUI)
