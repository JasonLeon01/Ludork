local Engine = require("Engine")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local TextLayout = Engine.TextLayout

local _DETAIL_TEXT_WIDTH = 320
local WindowShopDetailUI = {}

function WindowShopDetailUI:init(model, size, instance)
    self._size = size
    model._itemInfo = nil
    model._price = nil
    super(WindowShopDetailUI, self).init(model, instance)
end

function WindowShopDetailUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._descriptionControl = self:requireControl("Description")
    ---@cast self._descriptionControl Engine.PlainText
end

function WindowShopDetailUI:refresh()
    if self.model._itemInfo == nil then
        self:setText("ItemName", "")
        self:setText("Price", "")
        self:setText("Description", "")
    else
        self:setText("ItemName", LOC(self.model._itemInfo.name or ""))
        self:setText("Price", tostring(self.model._price or 0))
        local description = LOC(self.model._itemInfo.desc or ""):gsub("\\n", "\n")
        self:setText(
            "Description",
            TextLayout.wrapPlainText(description, _DETAIL_TEXT_WIDTH, self._descriptionControl)
        )
    end
    self.view:reflow(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowShopDetailUI:prepare()
    return super(WindowShopDetailUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowShopDetailUI:attach(nested)
    if nested == true then
        self:attachNestedWindowView(self.model)
    else
        self:attachWindowView(self.model)
    end
end

function WindowShopDetailUI:getWindowFrame()
    return self._windowFrame
end

function WindowShopDetailUI:getContent()
    return self._content
end

---@param itemInfo Source.Data.GeneralItemData | nil
---@param price    integer | nil
function WindowShopDetailUI:setItem(itemInfo, price)
    self.model._itemInfo = itemInfo
    self.model._price = price
    self:refresh()
end

return Ui.Define("Parts/WindowShop/WindowShopDetail", WindowShopDetailUI)
