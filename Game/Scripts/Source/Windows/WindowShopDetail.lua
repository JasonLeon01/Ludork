local Engine = require("Engine")
local WindowBase = require("Source.Windows.Base.WindowBase")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowShop.WindowShopDetail")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local TextLayout = Engine.TextLayout

---@class Source.Windows.WindowShopDetail.Controller
local Controller = {}

Controller.windowOptions = { focusable = false, hidden = true }

function Controller:init()
    self._itemInfo = nil
    self._price = nil
end

function Controller:refresh()
    if self._itemInfo == nil then
        self:setText("ItemName", "")
        self:setText("PriceLabel", "")
        self:setText("Price", "")
        self:setText("Description", "")
    else
        self:setText("ItemName", LOC(self._itemInfo.name or ""))
        self:setText("PriceLabel", LOC("price"))
        self:setText("Price", tostring(self._price or 0))
        local description = LOC(self._itemInfo.desc or ""):gsub("\\n", "\n")
        self:setText(
            "Description",
            TextLayout.wrapPlainText(
                description, self.ui.controls["DescriptionArea"]:getSize().x, self.ui.controls["Description"]
            )
        )
    end
    self.ui:prepare()
end

---@param itemInfo Source.Data.GeneralItemData | nil
---@param price    integer | nil
function Controller:setItem(itemInfo, price)
    self._itemInfo = itemInfo
    self._price = price
    self:refresh()
end

return Ui.DefineWindow(View, Controller, WindowBase)
