local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local IconTexture = require("Source.UI.IconTexture")
local Ui = require("Source.UI.Ui")
local ItemRowUI = require("Source.UI.Parts.WindowItem.ItemRow")

local ManagerFunctions = GlobalFunctions.Manager
local TextLayout = Engine.TextLayout
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local function orderedInventory(data, playerItems)
    local result = {}
    for _, itemID in ipairs(table.orderedStringKeys(data)) do
        if playerItems[itemID] ~= nil then
            result[#result + 1] = { itemID, playerItems[itemID] }
        end
    end
    return result
end

local WindowItemUI = {}

function WindowItemUI:init(model)
    super(WindowItemUI, self).init(model)
    self._logicalSize = nil
    self._rowUIs = {}
end

function WindowItemUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("ItemScrollBox")
    self._listView = self:requireControl("ItemList")
    self._descriptionControl = self:requireControl("Description")
    ---@cast self._descriptionControl Engine.PlainText
end

function WindowItemUI:attach()
    local windowSize = self.model:getSize()
    local logicalSize = sf.Vector2u.new(math.max(1, math.floor(windowSize.x)), math.max(1, math.floor(windowSize.y)))
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self:attachWindowView(self.model, self._logicalSize)
    self:_updateLayout()
    self.model._descNameText = self:requireControl("ItemName")
    self.model._descText = self:requireControl("Description")
    self.model:setScrollBox(self._scrollBox)
    self.model:setListView(self._listView)
end

function WindowItemUI:getWindowFrame()
    return self._windowFrame
end

function WindowItemUI:getContent()
    return self._content
end

function WindowItemUI:refresh()
    self:_assignDescription()
end

function WindowItemUI:refreshItems()
    self:_updateLayout()
    self._listView:clearChildren()
    self._rowUIs = {}
    local itemData = Data.GetAllGeneralItemData()
    local playerItems = self.model._player._items or {}
    local orderedItems = orderedInventory(itemData, playerItems)
    self.model._itemList = orderedItems
    for _, entry in ipairs(orderedItems) do
        local itemID = entry[1]
        local count = entry[2]
        local member = itemData[itemID] or {}
        local usable = member.usable
        if usable == nil then
            usable = true
        end
        local cost = member.cost
        if cost == nil then
            cost = true
        end
        local rowUI = ItemRowUI.new({
            iconTexture = IconTexture.LoadItem(member.icon or ""),
            usable = usable,
            cost = cost,
            count = count
        })
        local cell = rowUI:prepare(sf.Vector2u.new(32, 32))
        cell:addConfirmCallback(function ()
            self:_onUseItem()
        end)
        self._rowUIs[#self._rowUIs + 1] = rowUI
        self.model:_applyItem(cell)
        self._listView:addChild(cell)
    end
    self.model:resetSelection()
    self:updateDescription()
end

function WindowItemUI:tick()
    if self.model._lastDescIndex == self.model.index then
        return
    end
    self.model._lastDescIndex = self.model.index
    self:updateDescription()
end

function WindowItemUI:wrapDescription(text)
    return TextLayout.wrapPlainText(text, self.model._descMaxWidth, self._descriptionControl)
end

function WindowItemUI:updateDescription()
    self:_assignDescription()
    self.view:reflow(self._logicalSize)
end

function WindowItemUI:open()
    self:refreshItems()
    self.model:setVisible(true)
    self.model:setActive(true)
end

function WindowItemUI:close()
    self.model:setVisible(false)
    self.model:setActive(false)
end

function WindowItemUI:_onUseItem()
    if self.model.index == nil or self.model.index >= #self.model._itemList then
        return
    end
    local itemInfoData = Data.GetGeneralItemData(self.model._itemList[self.model.index + 1][1])
    local usable = itemInfoData.usable
    if usable == nil then
        usable = true
    end
    if not usable then
        return
    end
    ManagerFunctions.playSE(GameSystem.GetDecisionSE())
    local itemID = self.model._itemList[self.model.index + 1][1]
    local result = self.model._player:activateItem(itemID)
    assert(result.ok, "Item Ability failed: " .. tostring(result.code))
    self:close()
    if self.model._onUseCallback ~= nil then
        self.model._onUseCallback()
    end
end

function WindowItemUI:_closeByCancel()
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:close()
    if self.model._onCloseCallback ~= nil then
        self.model._onCloseCallback()
    end
end

function WindowItemUI:_updateLayout()
    local windowSize = self.model:getSize()
    local contentWidth = math.max(1, math.floor(windowSize.x - 32))
    local contentHeight = math.max(1, math.floor(windowSize.y - 96))
    self.model._descMaxWidth = contentWidth
    self._scrollBox:resize(sf.Vector2f.new(contentWidth, contentHeight))
    self._listView:setSize(sf.Vector2i.new(contentWidth, contentHeight))
end

function WindowItemUI:_assignDescription()
    if self.model.index == nil or self.model.index >= #self.model._itemList then
        self:setText("ItemName", "")
        self:setText("Description", "")
        return
    end
    local itemData = Data.GetGeneralItemData(self.model._itemList[self.model.index + 1][1])
    self:setText("ItemName", LOC(itemData.name or ""))
    local rawDescription = LOC(itemData.desc or "")
    self:setText("Description", self:wrapDescription(rawDescription))
end

return Ui.Define("WindowItem", WindowItemUI)
