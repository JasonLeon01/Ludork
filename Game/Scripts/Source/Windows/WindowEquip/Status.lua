local Engine = require("Engine")
local WindowBase = require("Source.Windows.Base.WindowBase")
local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEquip.WindowEquipStatusPane")
local EquipStatusRowController = require("Source.Windows.WindowEquip.EquipStatusRow.Controller")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local TextLayout = Engine.TextLayout

local _ATTR_ORDER = { "MAXHP", "HP", "ATK", "DEF", "EXP", "GOLD" }

---@class Source.Windows.WindowEquipStatus.Controller
local Controller = {}

Controller.windowOptions = { focusable = false, hidden = true }

function Controller:init(player)
    self._player = player
    self._descriptionName = ""
    self._descriptionText = ""
    self._showComparison = true
    self._changeRows = self:createCollection(
        self.ui.assets["StatusAsset"].controls["ChangeList"], EquipStatusRowController
    )
end

function Controller:refresh()
    self.ui.assets["StatusAsset"].instance:setText("ItemName", self._descriptionName)
    self.ui.assets["StatusAsset"].instance:setText("Description", self._descriptionText)
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:openForSlot(slotKey)
    self:refreshForSlot(slotKey)
    self.host:setVisible(true)
    self.host:setActive(false)
end

function Controller:close()
    self.host:setVisible(false)
    self.host:setActive(false)
end

function Controller:refreshForEquip(slotKey, candidateEquipID, showUnequip)
    if showUnequip == nil then
        showUnequip = false
    end
    local currentEquipID = self._player:getEquipInfo(slotKey)
    local currentAttrs = self:getAttrPlus(currentEquipID)
    local candidateAttrs = showUnequip and {} or self:getAttrPlus(candidateEquipID)
    self:refreshChangeRows(currentAttrs, candidateAttrs)
    self._showComparison = true
    self:refreshDescription(candidateEquipID, showUnequip)
end

function Controller:refreshForSlot(slotKey)
    self:clearChangeTexts()
    self._showComparison = false
    local currentEquipID = self._player:getEquipInfo(slotKey)
    self:refreshDescription(bool(currentEquipID) and currentEquipID or nil, false)
end

function Controller:refreshChangeRows(currentAttrs, candidateAttrs)
    self:clearChangeTexts()
    local list = self.ui.assets["StatusAsset"].controls["ChangeList"]
    local maximumRows = math.floor(list:getSize().y / list:getDefaultItemSize().y)
    local rowIndex = 0
    for _, attrKey in ipairs(self:getAttrKeys(candidateAttrs, currentAttrs)) do
        local delta = (candidateAttrs[attrKey] or 0) - (currentAttrs[attrKey] or 0)
        if delta ~= 0 then
            self._changeRows:add({ label = LOC(attrKey), delta = delta })
            rowIndex = rowIndex + 1
            if rowIndex >= maximumRows then
                break
            end
        end
    end
end

function Controller:refreshDescription(candidateEquipID, showUnequip)
    local descMaxWidth = self.ui.assets["StatusAsset"].controls["DescriptionArea"]:getSize().x
    if showUnequip then
        self._descriptionName = LOC("EQUIP_UNEQUIP")
        self._descriptionText = TextLayout.wrapPlainText(
            LOC("EQUIP_UNEQUIP_DESC"), descMaxWidth, self.ui.assets["StatusAsset"].controls["Description"]
        )
    elseif not bool(candidateEquipID) then
        self._descriptionName = ""
        self._descriptionText = ""
    else
        ---@cast candidateEquipID string
        local equipInfo = Data.GetGeneralEquipData(candidateEquipID)
        self._descriptionName = LOC(equipInfo.name or "")
        self._descriptionText = TextLayout.wrapPlainText(
            LOC(equipInfo.desc or ""), descMaxWidth, self.ui.assets["StatusAsset"].controls["Description"]
        )
    end
    self:refresh()
    self.ui.assets["StatusAsset"].instance:reflow()
    self:_applyDescriptionPosition()
end

function Controller:clearChangeTexts()
    self._changeRows:clear()
end

function Controller:_applyDescriptionPosition()
    if not self._showComparison then
        local position = self.ui.assets["StatusAsset"].controls["ChangeList"]:getPosition()
        local rootSize = self.ui.assets["StatusAsset"].root:getSize()
        local size = sf.Vector2f.new(rootSize.x, rootSize.y) - position
        self.ui.assets["StatusAsset"].instance:reflowControl(
            "DescriptionArea", sf.Vector2u.new(math.floor(size.x), math.floor(size.y))
        )
        self.ui.assets["StatusAsset"].controls["DescriptionArea"]:setPosition(position)
    end
end

---@diagnostic disable-next-line: unused
function Controller:getAttrPlus(equipID)
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
function Controller:getAttrKeys(firstAttrs, secondAttrs)
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

return Ui.DefineWindow(View, Controller, WindowBase)
