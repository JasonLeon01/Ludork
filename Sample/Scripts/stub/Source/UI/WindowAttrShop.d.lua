---@meta Source.UI.WindowAttrShop

---@class Source.UI.WindowAttrShop: Source.UI.UiController, Class.ClassType<Source.UI.WindowAttrShop>
---@field model                 Source.Windows.WindowAttrShop
---@field _selectable           Source.Windows.WindowAttrShop.Selectable | nil
---@field _logicalSize          sf.Vector2u | nil
---@field _shopNameSource       string
---@field _descriptionSource    string
---@field _shopName             string
---@field _description          string
---@field _priceTextValue       string
---@field _rows                 Source.UI.Parts.WindowAttrShop.AttrShopRow[]
---@field _windowFrame          Engine.Window
---@field _content              Engine.Canvas
---@field _scrollBox            Engine.ScrollBox
---@field _listView             Engine.ListView
---@field new                   fun(model: Source.Windows.WindowAttrShop): Source.UI.WindowAttrShop
---@field _avatarTexture        sf.Texture | nil
---@field _avatarRect           sf.IntRect | nil
---@field _avatarAnimatable     boolean
---@field _avatarSwitchInterval number
---@field _avatarSwitchTimer    number
---@field _avatarImage          Engine.Image
---@field _nameText             Engine.PlainText
---@field _descText             Engine.PlainText
---@field _priceText            Engine.PlainText
---@field _offers               Source.Windows.WindowAttrShop.Offer[]
local WindowAttrShopUI = {}

---@param model Source.Windows.WindowAttrShop
function WindowAttrShopUI:init(model) end

function WindowAttrShopUI:bind() end

function WindowAttrShopUI:refresh() end

---@param selectable Source.Windows.WindowAttrShop.Selectable
---@param size       sf.Vector2i
function WindowAttrShopUI:attachSelectable(selectable, size) end

---@return Engine.Window
function WindowAttrShopUI:getWindowFrame() end

---@return Engine.Canvas
function WindowAttrShopUI:getContent() end

---@return Engine.ListView
function WindowAttrShopUI:getListView() end

---@return Engine.ScrollBox
function WindowAttrShopUI:getScrollBox() end

---@return Source.Windows.WindowAttrShop.Selectable
function WindowAttrShopUI:_getSelectable() end

function WindowAttrShopUI:tick(deltaTime) end

---@param attributeName string
---@return string
function WindowAttrShopUI:getAttributeDisplayName(attributeName) end

function WindowAttrShopUI:refreshLocale() end

function WindowAttrShopUI:refreshPriceText() end

function WindowAttrShopUI:refreshItems() end

---@param onHidden function | nil
function WindowAttrShopUI:close(onHidden) end

function WindowAttrShopUI:closeByCancel() end

function WindowAttrShopUI:confirmItem() end

---@param shopActor Engine.Actor | nil
function WindowAttrShopUI:refreshAvatar(shopActor) end

---@param deltaTime number
function WindowAttrShopUI:animateAvatar(deltaTime) end

---@param abilityKey       string
---@param delta            integer
---@param price            integer
---@param moneyDisplayName string | nil
---@return string
function WindowAttrShopUI:formatPurchaseText(abilityKey, delta, price, moneyDisplayName) end

function WindowAttrShopUI:closeAndNotify() end

---@param size integer
---@return sf.IntRect
function WindowAttrShopUI.GetDefaultRect(size) end

function WindowAttrShopUI:refreshRows() end

---@param shopActor       Engine.Actor | nil
---@param shopName        string
---@param shopDescription string
---@param rect            sf.IntRect | nil
function WindowAttrShopUI:open(shopActor, shopName, shopDescription, rect) end

---@return string | nil
function WindowAttrShopUI:getSelectedAbilityKey() end

---@return boolean
function WindowAttrShopUI:isCurrentAvailable() end

---@type Source.UI.WindowAttrShop & Class.ClassType<Source.UI.WindowAttrShop>
local FinalWindowAttrShopUI = WindowAttrShopUI

return FinalWindowAttrShopUI
