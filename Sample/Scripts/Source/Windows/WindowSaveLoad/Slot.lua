local WindowSaveSlotUI = require("Source.UI.Parts.WindowSaveLoad.WindowSaveSlot")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local _SLOT_ROW_HEIGHT = 32
local WindowSaveSlot = {}

WindowSaveSlot.MAX_SAVE_SLOTS = 100

function WindowSaveSlot:init(rect, owner)
    super(WindowSaveSlot, self).init(rect, nil, rect.size.x - 64, _SLOT_ROW_HEIGHT, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._owner = owner
    self._ui = WindowSaveSlotUI.new(self, rect.size, WindowSaveSlot.MAX_SAVE_SLOTS)
    self._ui:attach()
    self._listView = self._ui:getListView()
end

function WindowSaveSlot:onTick(deltaTime)
    super(WindowSaveSlot, self).onTick(deltaTime)
    self._owner:notifySlotIndexMaybeChanged(self.index)
end

function WindowSaveSlot:onReturn()
    self._owner:cancelSlotSelection()
end

return class(WindowSaveSlot, WindowSelectable)
