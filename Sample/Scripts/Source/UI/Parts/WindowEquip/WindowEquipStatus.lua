local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local TextLayout = require("Source.TextLayout")
local Ui = require("Source.UI.Ui")
local EquipStatusRowUI = require("Source.UI.Parts.WindowEquip.EquipStatusRow")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _ROW_HEIGHT = 20
local _MAX_ROWS = 3
local _SLOT_DESC_NAME_Y = 0
local _SLOT_DESC_TEXT_Y = 24
local _EQUIP_DESC_NAME_Y = 76
local _EQUIP_DESC_TEXT_Y = 100

local _ATTR_ORDER = { "MAXHP", "HP", "ATK", "DEF", "EXP", "GOLD" }

---@class Source.UI.Parts.WindowEquip.WindowEquipStatus: Source.UI.UiController
local WindowEquipStatusUI = {}
---@type function
local wrapDescription

function WindowEquipStatusUI:init(model, instance)
    super(WindowEquipStatusUI, self).init(model, instance)
    self._changeRowControllers = {}
    self._descriptionName = ""
    self._descriptionText = ""
    self._descriptionNameY = _EQUIP_DESC_NAME_Y
    self._descriptionTextY = _EQUIP_DESC_TEXT_Y
    self._logicalSize = nil
end

function WindowEquipStatusUI:bind()
    local changeList = self:requireControl("ChangeList")
    ---@cast changeList Engine.ListView
    changeList:clearChildren()
    self._changeList = changeList
    self.model._descNameText = self:requireControl("ItemName")
    self.model._descText = self:requireControl("Description")
    self._descriptionControl = self.model._descText
    ---@cast self._descriptionControl Engine.PlainText
end

function WindowEquipStatusUI:refresh()
    self:setText("ItemName", self._descriptionName)
    self:setText("Description", self._descriptionText)
end

function WindowEquipStatusUI:attach(nested)
    self:_refreshLogicalSize()
    local root = self:prepare(self._logicalSize)
    self:_applyDescriptionPosition()
    if nested ~= true then
        ---@cast self.model.content Engine.Canvas
        self.model.content:addChild(root)
    end
end

function WindowEquipStatusUI:setPlayer(player)
    self.model._player = player
end

function WindowEquipStatusUI:openForSlot(slotKey)
    self:refreshForSlot(slotKey)
    self.model:setVisible(true)
    self.model:setActive(false)
end

function WindowEquipStatusUI:close()
    self.model:setVisible(false)
    self.model:setActive(false)
end

function WindowEquipStatusUI:refreshForEquip(slotKey, candidateEquipID, showUnequip)
    if showUnequip == nil then
        showUnequip = false
    end
    self:_refreshLogicalSize()
    self.model._slotKey = slotKey
    local currentEquipID = self.model._player:getEquipInfo(slotKey)
    local currentAttrs = self:getAttrPlus(currentEquipID)
    local candidateAttrs = showUnequip and {} or self:getAttrPlus(candidateEquipID)
    self:refreshChangeRows(currentAttrs, candidateAttrs)
    self._descriptionNameY = _EQUIP_DESC_NAME_Y
    self._descriptionTextY = _EQUIP_DESC_TEXT_Y
    self:refreshDescription(candidateEquipID, showUnequip)
end

function WindowEquipStatusUI:refreshForSlot(slotKey)
    self:_refreshLogicalSize()
    self.model._slotKey = slotKey
    self:clearChangeTexts()
    self._descriptionNameY = _SLOT_DESC_NAME_Y
    self._descriptionTextY = _SLOT_DESC_TEXT_Y
    local currentEquipID = self.model._player:getEquipInfo(slotKey)
    self:refreshDescription(bool(currentEquipID) and currentEquipID or nil, false)
end

function WindowEquipStatusUI:refreshChangeRows(currentAttrs, candidateAttrs)
    self:clearChangeTexts()
    local rowIndex = 0
    for _, attrKey in ipairs(self:getAttrKeys(candidateAttrs, currentAttrs)) do
        local delta = (candidateAttrs[attrKey] or 0) - (currentAttrs[attrKey] or 0)
        if delta ~= 0 then
            self:addChangeRow(attrKey, delta, rowIndex)
            rowIndex = rowIndex + 1
            if rowIndex >= _MAX_ROWS then
                break
            end
        end
    end
end

function WindowEquipStatusUI:addChangeRow(attrKey, delta, _rowIndex)
    local controller = EquipStatusRowUI.new({
        label = LOC(attrKey),
        delta = delta
    })
    ---@cast self._logicalSize sf.Vector2u
    local rowRoot = controller:prepare(sf.Vector2u.new(self._logicalSize.x, _ROW_HEIGHT))
    ---@cast self._changeList Engine.ListView
    self._changeList:addChild(rowRoot)
    self._changeList:applyPositions()
    self._changeRowControllers[#self._changeRowControllers + 1] = controller
    self.model._changeTexts[#self.model._changeTexts + 1] = controller:requireControl("Label")
    self.model._changeTexts[#self.model._changeTexts + 1] = controller:requireControl("Delta")
end

function WindowEquipStatusUI:refreshDescription(candidateEquipID, showUnequip)
    local descMaxWidth = math.max(1, math.floor(self.model.content:getSize().x))
    if showUnequip then
        self._descriptionName = LOC("EQUIP_UNEQUIP")
        self._descriptionText = wrapDescription(
            LOC("EQUIP_UNEQUIP_DESC"), descMaxWidth, self._descriptionControl
        )
    elseif not bool(candidateEquipID) then
        self._descriptionName = ""
        self._descriptionText = ""
    else
        ---@cast candidateEquipID string
        local equipInfo = Data.GetGeneralEquipData(candidateEquipID)
        self._descriptionName = LOC(equipInfo.name or "")
        self._descriptionText = wrapDescription(
            LOC(equipInfo.desc or ""), descMaxWidth, self._descriptionControl
        )
    end
    self:refresh()
    self.view:reflow(self._logicalSize)
    self:_applyDescriptionPosition()
end

function WindowEquipStatusUI:clearChangeTexts()
    ---@cast self._changeList Engine.ListView
    for _, controller in ipairs(self._changeRowControllers) do
        local rowRoot = controller:getRoot()
        if rowRoot:getParent() == self._changeList then
            self._changeList:removeChild(rowRoot)
        end
    end
    self._changeRowControllers = {}
    self.model._changeTexts = {}
end

function WindowEquipStatusUI:setDescriptionPosition(nameY, descY)
    self._descriptionNameY = nameY
    self._descriptionTextY = descY
    self:_applyDescriptionPosition()
end

function WindowEquipStatusUI:_applyDescriptionPosition()
    self.model._descNameText:setPosition(sf.Vector2f.new(0.0, self._descriptionNameY))
    self.model._descText:setPosition(sf.Vector2f.new(0.0, self._descriptionTextY))
end

---@diagnostic disable-next-line: unused
function WindowEquipStatusUI:getAttrPlus(equipID)
    if not bool(equipID) then
        return {}
    end
    ---@cast equipID string
    local attrPlus = Data.GetGeneralEquipData(equipID).attrPlus
    if attrPlus == nil then
        return {}
    end
    return copy(attrPlus)
end

---@diagnostic disable-next-line: unused
function WindowEquipStatusUI:getAttrKeys(firstAttrs, secondAttrs)
    local result = {}
    local included = {}
    for _, attrs in ipairs({ firstAttrs, secondAttrs }) do
        for _, attrKey in ipairs(table.orderedStringKeys(attrs, _ATTR_ORDER)) do
            if not included[attrKey] then
                result[#result + 1] = attrKey
                included[attrKey] = true
            end
        end
    end
    return result
end

function WindowEquipStatusUI:setRightAligned(text, y)
    local bounds = text:getLocalBounds()
    local contentWidth = self.model.content:getSize().x
    text:setPosition(sf.Vector2f.new(contentWidth - bounds.size.x - bounds.position.x, y))
end

function wrapDescription(text, maxWidth, control)
    return TextLayout.WrapPlainText(text, maxWidth, control)
end

function WindowEquipStatusUI:_refreshLogicalSize()
    local contentSize = self.model.content:getSize()
    local logicalSize = sf.Vector2u.new(math.max(1, math.floor(contentSize.x)), math.max(1, math.floor(contentSize.y)))
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
end

return Ui.Define("Parts/WindowEquip/WindowEquipStatus", WindowEquipStatusUI)
