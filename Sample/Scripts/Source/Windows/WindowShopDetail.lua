local WindowShopDetailUI = require("Source.UI.Parts.WindowShop.WindowShopDetail")
local WindowBase = require("Source.Windows.Base.WindowBase")

---@class Source.Windows.WindowShopDetail
local WindowShopDetail = {}

function WindowShopDetail:init(rect, instance)
    super(WindowShopDetail, self).init(rect, nil, nil, true)
    self:setCanReceiveFocus(false)
    self._ui = WindowShopDetailUI.new(self, rect.size, instance)
    self._ui:attach(instance ~= nil)
end

---@param itemInfo Source.Data.GeneralItemData | nil
---@param price    integer | nil
function WindowShopDetail:setItem(itemInfo, price)
    self._ui:setItem(itemInfo, price)
end

function WindowShopDetail:refresh()
    self._ui:refresh()
end

function WindowShopDetail:dispose()
    self._ui:dispose()
    self._ui = nil
end

return class(WindowShopDetail, WindowBase)
