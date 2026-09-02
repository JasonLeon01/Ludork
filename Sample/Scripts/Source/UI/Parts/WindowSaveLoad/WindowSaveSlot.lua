local EventKeys = require("Source.Configs.EventKeys")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")
local WindowSaveSlotRowUI = require("Source.UI.Parts.WindowSaveLoad.WindowSaveSlotRow")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local WindowSaveSlotUI = {}

WindowSaveSlotUI.refreshEvents = { EventKeys.LocaleChanged }

function WindowSaveSlotUI:init(model, size, maxSlots, instance)
    self._size = size
    self._maxSlots = maxSlots
    self._slotItems = {}
    self._slotRowUIs = {}
    super(WindowSaveSlotUI, self).init(model, instance)
end

function WindowSaveSlotUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("SlotScrollBox")
    self._listView = self:requireControl("SlotList")
    self._listView:clearChildren()
    self._slotItems = {}
    for slotIndex = 0, self._maxSlots - 1 do
        local slot = slotIndex
        local rowUI = WindowSaveSlotRowUI.new({
            text = LOC("SAVEFILE"):pformat(slot + 1),
            callback = function (_obj, _kwargs)
                self.model._owner:onSlotConfirm(slot)
            end
        })
        local root = rowUI:prepare()
        self._slotRowUIs[#self._slotRowUIs + 1] = rowUI
        self.model:_applyItem(root)
        self._listView:addChild(root)
        self._slotItems[#self._slotItems + 1] = root
    end
end

function WindowSaveSlotUI:refresh()
    for slotIndex, rowUI in ipairs(self._slotRowUIs) do
        rowUI.model.text = LOC("SAVEFILE"):pformat(slotIndex)
        rowUI:refresh()
    end
end

function WindowSaveSlotUI:prepare()
    return super(WindowSaveSlotUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowSaveSlotUI:attach(nested)
    if nested == true then
        self:attachNestedWindowView(self.model)
    else
        self:attachWindowView(self.model)
    end
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

function WindowSaveSlotUI:getScrollBox()
    return self._scrollBox
end

function WindowSaveSlotUI:dispose()
    for _, rowUI in ipairs(self._slotRowUIs) do
        rowUI:dispose()
    end
    self._slotRowUIs = {}
    self._slotItems = {}
    self._scrollBox = nil
    self._listView = nil
    super(WindowSaveSlotUI, self).dispose()
end

return Ui.Define("Parts/WindowSaveLoad/WindowSaveSlot", WindowSaveSlotUI)
