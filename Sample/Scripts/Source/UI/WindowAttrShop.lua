local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local AttrShopRowUI = require("Source.UI.Parts.WindowAttrShop.AttrShopRow")
local Ui = require("Source.UI.Ui")
local UiLayout = require("Source.UI.UiLayout")

local AudioManager = GlobalCore.AudioManager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _AVATAR_SIZE = 32
local _ITEM_ROW_HEIGHT = 32

---@class Source.UI.WindowAttrShop
local WindowAttrShopUI = {}

function WindowAttrShopUI:init(model)
    super(WindowAttrShopUI, self).init(model)
    self._selectable = nil
    self._logicalSize = nil
    self._shopNameSource = ""
    self._descriptionSource = ""
    self._shopName = ""
    self._description = ""
    self._priceTextValue = ""
    self._rows = {}
    self._offers = {}
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarAnimatable = false
    self._avatarSwitchInterval = 0.2
    self._avatarSwitchTimer = 0.0
end

function WindowAttrShopUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("AbilityScrollBox")
    self._listView = self:requireControl("AbilityList")
    self._avatarImage = self:requireControl("Avatar")
    self._nameText = self:requireControl("ShopName")
    self._descText = self:requireControl("Description")
    self._priceText = self:requireControl("Price")
end

function WindowAttrShopUI:refresh()
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:setText("Price", self._priceTextValue)
    self:setProperty("Avatar", "visible", false)
end

function WindowAttrShopUI:attachSelectable(selectable, size)
    self._selectable = selectable
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self:attachWindowView(selectable, self._logicalSize)
end

function WindowAttrShopUI:getWindowFrame()
    return self._windowFrame
end

function WindowAttrShopUI:getContent()
    return self._content
end

function WindowAttrShopUI:getListView()
    return self._listView
end

function WindowAttrShopUI:getScrollBox()
    return self._scrollBox
end

---@return Source.Windows.WindowAttrShop.Selectable
function WindowAttrShopUI:_getSelectable()
    assert(self._selectable ~= nil, "Attribute shop selectable has not been attached")
    return self._selectable
end

function WindowAttrShopUI:refreshRows()
    local selectable = self:_getSelectable()
    local previousIndex = selectable.index
    self._offers = self.model:getOffers()
    self._listView:clearChildren()
    self._rows = {}
    local cellWidth = selectable:getItemWidth()
    local moneyDisplayName = self:getAttributeDisplayName(self.model:getCurrencyName())
    for _, offer in ipairs(self._offers) do
        self:_addRow(
            self:formatPurchaseText(offer.key, offer.delta, offer.price, moneyDisplayName), offer.available, cellWidth
        )
    end
    self:_addRow(LOC("SHOP_ATTR_LEAVE"), true, cellWidth)
    if previousIndex == nil then
        selectable.index = 0
    else
        selectable.index = math.trunc(math.min(previousIndex, #self._offers))
    end
    self:_reflow()
    selectable:detachSelectionRect()
end

function WindowAttrShopUI:getSelectedAbilityKey()
    local index = self:_getSelectable().index
    local offer = index ~= nil and self._offers[index + 1] or nil
    return offer ~= nil and offer.key or nil
end

function WindowAttrShopUI:isCurrentAvailable()
    local index = self:_getSelectable().index
    if index == nil or index < 0 or index > #self._offers then
        return false
    end
    return index == #self._offers or assert(self._offers[index + 1]).available
end

---@param textValue string
---@param available boolean
---@param width     integer
function WindowAttrShopUI:_addRow(textValue, available, width)
    local row = AttrShopRowUI.new({
        text = textValue,
        available = available
    })
    local logicalSize = sf.Vector2u.new(width, _ITEM_ROW_HEIGHT)
    ---@cast logicalSize sf.Vector2u
    local cell = row:prepare(logicalSize)
    cell:addConfirmCallback(function ()
        self:confirmItem()
    end)
    self._rows[#self._rows + 1] = row
    self._listView:addChild(cell)
end

function WindowAttrShopUI:tick(deltaTime)
    self:animateAvatar(deltaTime)
end

---@diagnostic disable-next-line: unused
function WindowAttrShopUI:getAttributeDisplayName(attributeName)
    return LOC
        (attributeName)
        :gsub("%s+$", "")
        :gsub("[:：]+$", "")
end

function WindowAttrShopUI:open(shopActor, shopName, shopDescription, rect)
    local selectable = self:_getSelectable()
    if rect ~= nil then
        selectable:setPosition(sf.Vector2f.new(rect.position.x, rect.position.y))
    end
    self:refreshAvatar(shopActor)
    self._shopNameSource = tostring(shopName or "")
    self._descriptionSource = tostring(shopDescription or "")
    self:refreshLocale()
    selectable:resetSelection()
    selectable:showWithAnimation("FadeIn", function ()
        selectable:setActive(true)
        selectable:requestKeyboardFocus()
    end)
end

function WindowAttrShopUI:refreshLocale()
    self._shopName = bool(self._shopNameSource) and LOC(self._shopNameSource) or ""
    local description = bool(self._descriptionSource) and LOC(self._descriptionSource) or ""
    self._description = description:gsub("\\n", "\n")
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:refreshPriceText()
    self:refreshItems()
end

function WindowAttrShopUI:refreshPriceText()
    local priceValue = self.model:getSharedPrice()
    if priceValue == nil then
        self._priceTextValue = ""
    else
        self._priceTextValue = Engine.ApplyStringMappingFormat(LOC("SHOP_ATTR_PRICE"), {
            gold = math.trunc(tonumber(priceValue) or 0)
        })
    end
    self:setText("Price", self._priceTextValue)
    self:_reflow()
end

function WindowAttrShopUI:refreshItems()
    self:refreshRows()
end

function WindowAttrShopUI:close(onHidden)
    local selectable = self:_getSelectable()
    selectable:setActive(false)
    selectable:hideWithAnimation("FadeOut", function ()
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowAttrShopUI:closeByCancel()
    if self.model:isClosed() then
        return
    end
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:closeAndNotify()
end

function WindowAttrShopUI:confirmItem()
    local selectable = self:_getSelectable()
    local abilityKey = selectable:getSelectedAbilityKey()
    if abilityKey == nil then
        self:closeByCancel()
        return
    end
    if not self.model:purchaseAttribute(abilityKey) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        self:refreshItems()
        return
    end
    AudioManager.playSound(GameSystem.GetShopSE())
    self:refreshPriceText()
    self:refreshItems()
end

function WindowAttrShopUI:refreshAvatar(shopActor)
    self:setProperty("Avatar", "visible", false)
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarAnimatable = false
    self._avatarSwitchTimer = 0.0
    if shopActor == nil then
        self:_reflow()
        return
    end
    local texture = shopActor:getTexture()
    if texture == nil then
        self:_reflow()
        return
    end
    local sourceRect = shopActor:getTextureRect()
    local textureRect = copy(sourceRect)
    local frameSize = textureRect.size
    if frameSize.x <= 0 or frameSize.y <= 0 then
        self:_reflow()
        return
    end
    self._avatarTexture = texture
    self._avatarRect = textureRect
    self._avatarAnimatable = shopActor:getAnimatable()
    self._avatarSwitchInterval = shopActor.switchInterval
    self._avatarImage:setTexture(texture, false)
    self._avatarImage:setTextureRect(textureRect)
    self:setProperty("Avatar", "visible", true)
    self:_reflow()
end

function WindowAttrShopUI:animateAvatar(deltaTime)
    if not self._avatarAnimatable or not self._avatarImage:getVisible()
        or self._avatarTexture == nil or self._avatarRect == nil then
        return
    end
    self._avatarSwitchTimer = self._avatarSwitchTimer + deltaTime
    if self._avatarSwitchTimer < self._avatarSwitchInterval then
        return
    end
    self._avatarSwitchTimer = 0.0
    local textureWidth = self._avatarTexture:getSize().x
    local positionX = (self._avatarRect.position.x + self._avatarRect.size.x) % textureWidth
    ---@cast positionX integer
    local avatarRect = sf.IntRect.new(
        positionX, self._avatarRect.position.y, self._avatarRect.size.x, self._avatarRect.size.y
    )
    ---@cast avatarRect sf.IntRect
    self._avatarRect = avatarRect
    self._avatarImage:setTextureRect(self._avatarRect)
end

function WindowAttrShopUI:formatPurchaseText(abilityKey, delta, price, moneyDisplayName)
    local priceValue = self.model:getSharedPrice()
    if priceValue ~= nil then
        return tostring(delta) .. " " .. self:getAttributeDisplayName(abilityKey)
    end
    moneyDisplayName = moneyDisplayName or self:getAttributeDisplayName(self.model:getCurrencyName())
    return tostring(price) .. " " .. moneyDisplayName .. " :  " .. tostring(delta) .. " "
        .. self:getAttributeDisplayName(abilityKey)
end

function WindowAttrShopUI:closeAndNotify()
    self.model:close(true)
end

function WindowAttrShopUI.GetDefaultRect(size)
    return UiLayout.GetCenteredRect(size, size)
end

function WindowAttrShopUI:_reflow()
    self.view:reflow(self._logicalSize)
    if self._avatarRect == nil then
        return
    end
    self._avatarImage:setScale(
        sf.Vector2f.new(_AVATAR_SIZE / self._avatarRect.size.x, _AVATAR_SIZE / self._avatarRect.size.y)
    )
end

return Ui.Define("WindowAttrShop", WindowAttrShopUI)
