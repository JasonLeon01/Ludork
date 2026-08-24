local GameSystem = require("Source.System")
local ConfigRowControllerBase = require("Source.UI.Parts.ConfigWindow.ConfigRow")
local Ui = require("Source.UI.Ui")

local _ROW_HEIGHT = 32

local ConfigSettingRowUI = {}

function ConfigSettingRowUI:init(labelText, items, rowWidth, dropboxWidth, windowSkin, selectedIndex)
    self._labelText = labelText
    self._items = items
    self._dropboxWidth = dropboxWidth
    self._windowSkin = windowSkin
    if selectedIndex == nil then
        selectedIndex = 0
    end
    self._selectedIndex = selectedIndex
    super(ConfigSettingRowUI, self).init(nil, rowWidth, _ROW_HEIGHT)
end

function ConfigSettingRowUI:bind()
    self._label = self:requireControl("Label")
    self._dropBox = self:requireControl("DropBox")
    self._dropBox:setCanReceiveFocus(false)
    self._dropBox:resize(sf.Vector2f.new(self._dropboxWidth, _ROW_HEIGHT))
    self._dropBox:setWindowSkin(self._windowSkin)
    self._dropBox:setItems(self._items)
    self._dropBox:setSelectedIndex(self._selectedIndex)
    self._dropBox:setOpenSound(GameSystem.GetDecisionSE())
    self._dropBox:setCursorSound(GameSystem.GetCursorSE())
    self._dropBox:setSelectSound(GameSystem.GetDecisionSE())
    self._dropBox:setCancelSound(GameSystem.GetCancelSE())
end

function ConfigSettingRowUI:refresh()
    self:setText("Label", self._labelText)
end

function ConfigSettingRowUI:getDropBox()
    return self._dropBox
end

function ConfigSettingRowUI:setItems(items)
    self._items = items
    self._dropBox:setItems(items)
    self:prepare()
end

function ConfigSettingRowUI:setActive(active)
    super(ConfigSettingRowUI, self).setActive(active)
    self._dropBox:setActive(active)
end

function ConfigSettingRowUI:dispose()
    if self._dropBox ~= nil then
        self._dropBox:setOnExpandedChanged(nil)
        self._dropBox:setOnSelectedIndexChanged(nil)
        self._dropBox:setOnSelectionConfirmed(nil)
    end
    self.root:addConfirmCallback(nil)
    self._dropBox = nil
    super(ConfigSettingRowUI, self).dispose()
end

return Ui.Define("Parts/ConfigWindow/ConfigSettingRow", ConfigSettingRowUI, ConfigRowControllerBase)
