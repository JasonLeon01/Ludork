local GlobalCore = require("GlobalCore")
local Ui = require("Internal.UIBase.Ui")
local WindowSelectable = require("Internal.UIBase.WindowSelectable")
local View = require("Internal.UI.WindowShopRecords")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local MapDisplayName = require("Source.Utils.MapDisplayName")
local SelectableTextRowController = require("Source.Windows.Shared.SelectableTextRow.Controller")

local AudioManager = GlobalCore.AudioManager

---@class Source.Windows.WindowShopRecords.Controller
local Controller = {}

Controller.windowOptions = {
    centered = true,
    hidden = true,
    returnButton = true,
    list = "ShopList",
    scroll = "ShopScrollBox"
}

function Controller:init(onConfirm, onClose)
    self._onConfirmCallback = onConfirm
    self._onCloseCallback = onClose
    self._shops = {}
    self._currentMapPath = nil
    self._rows = self:createCollection(self.ui.controls["ShopList"], SelectableTextRowController)
end

function Controller:getVisible()
    return self._transition:isBlocking()
end

function Controller:open(shops, currentMapPath)
    if #shops == 0 then
        return false
    end
    self._shops = deepcopy(shops)
    self._currentMapPath = currentMapPath
    self._rows:clear()
    for _, shop in ipairs(self._shops) do
        self._rows:add({
            text = self:formatShopName(shop),
            onConfirm = self:bindCallback(Controller.confirmShop)
        })
    end
    self._rows:layout()
    self.host:resetSelection()
    self._transition:show("FadeIn", function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocus()
    end)
    return true
end

function Controller:formatShopName(shop)
    return MapDisplayName.Resolve(shop[1], self._currentMapPath) .. " " .. LocaleCore.ApplyStringLocaleFormat(shop[2])
end

function Controller:refreshLocale()
    local scrollOffset = self.ui.controls["ShopScrollBox"]:getScrollOffset()
    for index, row in ipairs(self._rows.items) do
        row.model.text = self:formatShopName(assert(self._shops[index]))
        row:refresh()
    end
    self.ui.controls["ShopList"]:invalidatePositions()
    self._rows:layout()
    self.ui.controls["ShopScrollBox"]:setScrollOffset(scrollOffset)
end

function Controller:confirmShop()
    local index = self.host.index
    local shop = index ~= nil and self._shops[index + 1] or nil
    if shop ~= nil and self._onConfirmCallback ~= nil then
        self._onConfirmCallback(shop[1], shop[2])
    end
end

function Controller:close(onHidden)
    self._transition:hide("FadeOut", onHidden)
end

function Controller:dismiss()
    self._transition:hideImmediate()
end

function Controller:onReturn()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(self._onCloseCallback)
end

function Controller:dispose()
    self:dismiss()
    super(Controller, self).dispose()
    self._onConfirmCallback = nil
    self._onCloseCallback = nil
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
