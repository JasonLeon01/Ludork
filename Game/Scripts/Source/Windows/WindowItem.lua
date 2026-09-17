local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local IconTexture = require("Source.UIBase.IconTexture")
local Ui = require("Source.UIBase.Ui")
local UiLayout = require("Source.UIBase.UiLayout")
local WindowTransition = require("Source.UIBase.WindowTransition")
local View = require("Source.UI.WindowItem")
local ItemRowController = require("Source.Windows.WindowItem.ItemRow.Controller")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local AudioManager = GlobalCore.AudioManager
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

---@class Source.Windows.WindowItem.Controller
local Controller = {}

Controller.windowOptions = {
    position = sf.Vector2f.new(192, 0),
    hidden = true,
    returnButton = true,
    list = "ItemList",
    scroll = "ItemScrollBox",
    itemWidth = 32,
    itemHeight = 32
}

function Controller:init(player, onClose)
    self._onCloseCallback = onClose
    self._onUseCallback = nil
    self._player = player
    self._logicalSize = nil
    self._itemList = {}
    self._lastDescIndex = nil
    self._descMaxWidth = 1
    self._transitionProfile = WindowTransition.MENU
    self._rows = self:createCollection(self.ui.controls["ItemList"], ItemRowController)
end

function Controller:ready()
    self:_updateLayout()
    self:refreshItems()
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    self:tick()
end

function Controller:open(transitionProfile)
    self._transitionProfile = transitionProfile or WindowTransition.MENU
    local size = self.ui.root:getSize()
    if self._transitionProfile == WindowTransition.MENU then
        self.host:setPosition(UiLayout.GetMenuDockPosition())
    else
        self.host:setPosition(UiLayout.GetCenteredPosition(size.x, size.y))
    end
    self:refreshItems()
    local fadeIn = WindowTransition.GetAnimationNames(self._transitionProfile)
    self.host:showWithAnimation(fadeIn, function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocusAtCursor()
    end)
end

function Controller:refreshLocale()
    self:updateDescription()
end

function Controller:close(onHidden)
    self.host:setActive(false)
    local _, fadeOut = WindowTransition.GetAnimationNames(self._transitionProfile)
    self.host:hideWithAnimation(fadeOut, onHidden)
end

function Controller:onReturn()
    self:closeByCancel()
end

function Controller:getPlayer()
    return self._player
end

function Controller:setOnCloseCallback(callback)
    self._onCloseCallback = callback
end

function Controller:setOnUseCallback(callback)
    self._onUseCallback = callback
end

function Controller:onItemUsed()
    if self._onUseCallback ~= nil then
        self._onUseCallback()
    end
end

function Controller:notifyClosed()
    if self._onCloseCallback ~= nil then
        self._onCloseCallback()
    end
end

function Controller:refresh()
    self:_assignDescription()
end

function Controller:refreshItems()
    self:_updateLayout()
    self._rows:clear()
    local itemData = Data.GetAllGeneralItemData()
    local playerItems = self:getPlayer():getItems()
    local orderedItems = orderedInventory(itemData, playerItems)
    self._itemList = orderedItems
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
        local rowSize = sf.Vector2u.new(32, 32)
        ---@cast rowSize sf.Vector2u
        local rowUI = self._rows:add({
            iconTexture = IconTexture.Load(member.icon or ""),
            usable = usable,
            cost = cost,
            count = count
        }, rowSize)
        local cell = rowUI.ui.root
        cell:addConfirmCallback(self:bindCallback(Controller.useSelectedItem))
        self.host:applyItem(cell)
    end
    self._rows:layout()
    self.host:resetSelection()
    self:updateDescription()
end

function Controller:tick()
    if self._lastDescIndex == self.host.index then
        return
    end
    self._lastDescIndex = self.host.index
    self:updateDescription()
end

function Controller:wrapDescription(text)
    return TextLayout.wrapPlainText(text, self._descMaxWidth, self.ui.controls["Description"])
end

function Controller:updateDescription()
    self:_assignDescription()
    self.view:reflow(self._logicalSize)
end

function Controller:useSelectedItem()
    if self.host.index == nil or self.host.index >= #self._itemList then
        return
    end
    local itemInfoData = Data.GetGeneralItemData(assert(self._itemList[self.host.index + 1])[1])
    local usable = itemInfoData.usable
    if usable == nil then
        usable = true
    end
    if not usable then
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    local itemID = assert(self._itemList[self.host.index + 1])[1]
    local result = self:getPlayer():activateItem(itemID)
    assert(result.ok, "Item Ability failed: " .. tostring(result.code))
    self:close()
    self:onItemUsed()
end

function Controller:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(self:bindCallback(Controller.notifyClosed))
end

function Controller:_updateLayout()
    local windowSize = self.host:getSize()
    local contentWidth = math.max(1, math.floor(windowSize.x - 32))
    local contentHeight = math.max(1, math.floor(windowSize.y - 96))
    self._descMaxWidth = contentWidth
    self.ui.controls["ItemScrollBox"]:resize(sf.Vector2f.new(contentWidth, contentHeight))
    local size = sf.Vector2i.new(contentWidth, contentHeight)
    ---@cast size sf.Vector2i
    self.ui.controls["ItemList"]:setSize(size)
end

function Controller:_assignDescription()
    if self.host.index == nil or self.host.index >= #self._itemList then
        self:setText("ItemName", "")
        self:setText("Description", "")
        return
    end
    local itemData = Data.GetGeneralItemData(assert(self._itemList[self.host.index + 1])[1])
    self:setText("ItemName", LOC(itemData.name or ""))
    local rawDescription = LOC(itemData.desc or "")
    self:setText("Description", self:wrapDescription(rawDescription))
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
