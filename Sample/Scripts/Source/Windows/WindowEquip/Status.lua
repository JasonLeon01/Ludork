local WindowEquipStatusUI = require("Source.UI.Parts.WindowEquip.WindowEquipStatus")
local WindowEquipStatusPaneUI = require("Source.UI.Parts.WindowEquip.WindowEquipStatusPane")
local WindowBase = require("Source.Windows.Base.WindowBase")
---@class Source.Windows.WindowEquipStatus: Source.Windows.Base.WindowBase
local WindowEquipStatus = {}

WindowEquipStatus.uiClass = WindowEquipStatusUI

function WindowEquipStatus:init(rect, player, instance)
    super(WindowEquipStatus, self).init(rect, nil, nil, instance ~= nil)
    self._player = player
    self._slotKey = ""
    self._changeTexts = {}
    local statusInstance = nil
    if instance ~= nil then
        local paneUI = WindowEquipStatusPaneUI.new(self, instance)
        self._paneUI = paneUI
        paneUI:attach()
        statusInstance = paneUI:getStatusAsset()
    end
    self._statusUI = self.uiClass.new(self, statusInstance)
    self._statusUI:attach(statusInstance ~= nil)
    self:setActive(false)
    self:setVisible(false)
end

function WindowEquipStatus:setPlayer(player)
    self._player = player
    self._statusUI:setPlayer(player)
end

function WindowEquipStatus:openForSlot(slotKey)
    self._statusUI:openForSlot(slotKey)
end

function WindowEquipStatus:close()
    self._statusUI:close()
end

function WindowEquipStatus:refreshForEquip(slotKey, candidateEquipID, showUnequip)
    self._statusUI:refreshForEquip(slotKey, candidateEquipID, showUnequip)
end

function WindowEquipStatus:refreshForSlot(slotKey)
    self._statusUI:refreshForSlot(slotKey)
end

---@param currentAttrs   table
---@param candidateAttrs table
function WindowEquipStatus:_refreshChangeRows(currentAttrs, candidateAttrs)
    self._statusUI:refreshChangeRows(currentAttrs, candidateAttrs)
end

---@param attrKey  string
---@param delta    integer
---@param rowIndex integer
function WindowEquipStatus:_addChangeRow(attrKey, delta, rowIndex)
    self._statusUI:addChangeRow(attrKey, delta, rowIndex)
end

---@param candidateEquipID string | nil
---@param showUnequip      boolean
function WindowEquipStatus:_refreshDescription(candidateEquipID, showUnequip)
    self._statusUI:refreshDescription(candidateEquipID, showUnequip)
end

function WindowEquipStatus:_clearChangeTexts()
    self._statusUI:clearChangeTexts()
end

---@param nameY integer
---@param descY integer
function WindowEquipStatus:_setDescriptionPosition(nameY, descY)
    self._statusUI:setDescriptionPosition(nameY, descY)
end

---@param equipID string | nil
---@return table
function WindowEquipStatus:_getAttrPlus(equipID)
    return self._statusUI:getAttrPlus(equipID)
end

---@param firstAttrs  table
---@param secondAttrs table
---@return table
function WindowEquipStatus:_getAttrKeys(firstAttrs, secondAttrs)
    return self._statusUI:getAttrKeys(firstAttrs, secondAttrs)
end

---@param text Engine.PlainText
---@param y    number
function WindowEquipStatus:_setRightAligned(text, y)
    self._statusUI:setRightAligned(text, y)
end

return class(WindowEquipStatus, WindowBase)
