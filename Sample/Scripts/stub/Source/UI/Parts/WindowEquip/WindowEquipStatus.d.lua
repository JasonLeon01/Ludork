---@meta Source.UI.Parts.WindowEquip.WindowEquipStatus

---@class Source.UI.Parts.WindowEquip.WindowEquipStatus: Source.UI.UiController
---@field model Source.Windows.WindowEquipStatus
---@field _logicalSize sf.Vector2u | nil
---@field _changeRowControllers Source.UI.Parts.WindowEquip.EquipStatusRow[]
---@field new fun(model: Source.Windows.WindowEquipStatus): Source.UI.Parts.WindowEquip.WindowEquipStatus
local WindowEquipStatusUI = {}

---@param model Source.Windows.WindowEquipStatus
function WindowEquipStatusUI:init(model) end

function WindowEquipStatusUI:bind() end

function WindowEquipStatusUI:refresh() end

function WindowEquipStatusUI:attach() end

---@param player Source.Player.Player
function WindowEquipStatusUI:setPlayer(player) end

---@param slotKey string
function WindowEquipStatusUI:openForSlot(slotKey) end

function WindowEquipStatusUI:close() end

---@param slotKey          string
---@param candidateEquipID string | nil
---@param showUnequip      boolean | nil
function WindowEquipStatusUI:refreshForEquip(slotKey, candidateEquipID, showUnequip) end

---@param slotKey string
function WindowEquipStatusUI:refreshForSlot(slotKey) end

---@param currentAttrs   table<string, integer>
---@param candidateAttrs table<string, integer>
function WindowEquipStatusUI:refreshChangeRows(currentAttrs, candidateAttrs) end

---@param attrKey  string
---@param delta    integer
---@param rowIndex integer
function WindowEquipStatusUI:addChangeRow(attrKey, delta, rowIndex) end

---@param candidateEquipID string | nil
---@param showUnequip      boolean
function WindowEquipStatusUI:refreshDescription(candidateEquipID, showUnequip) end

function WindowEquipStatusUI:clearChangeTexts() end

---@param nameY integer
---@param descY integer
function WindowEquipStatusUI:setDescriptionPosition(nameY, descY) end

---@param equipID string | nil
---@return table<string, integer>
function WindowEquipStatusUI:getAttrPlus(equipID) end

---@param firstAttrs  table
---@param secondAttrs table
---@return table
function WindowEquipStatusUI:getAttrKeys(firstAttrs, secondAttrs) end

---@param text Engine.PlainText
---@param y    number
function WindowEquipStatusUI:setRightAligned(text, y) end

---@param text     string
---@param maxWidth number
---@return string
function WindowEquipStatusUI.wrapDescription(text, maxWidth) end

return WindowEquipStatusUI
