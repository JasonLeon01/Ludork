---@meta Source.UI.WindowAttrShop

---@class Source.UI.WindowAttrShop: Source.UI.UiController, Class.ClassType<Source.UI.WindowAttrShop>
---@field model              Source.Windows.WindowAttrShop
---@field _selectable        Source.Windows._WindowAttrShopSelectable | nil
---@field _logicalSize       sf.Vector2u | nil
---@field _shopNameSource    string
---@field _descriptionSource string
---@field _shopName          string
---@field _description       string
---@field _priceTextValue    string
---@field _rows              Source.UI.Parts.WindowAttrShop.AttrShopRow[]
---@field _windowFrame       Engine.Window
---@field _content           Engine.Canvas
---@field _scrollBox         Engine.ScrollBox
---@field _listView          Engine.ListView
---@field new                fun(model: Source.Windows.WindowAttrShop): Source.UI.WindowAttrShop
local WindowAttrShopUI = {}

---@param model Source.Windows.WindowAttrShop
function WindowAttrShopUI:init(model) end

function WindowAttrShopUI:bind() end

function WindowAttrShopUI:refresh() end

---@param selectable Source.Windows._WindowAttrShopSelectable
---@param size       sf.Vector2i
---@return Engine.Canvas
---@param selectable Source.Windows._WindowAttrShopSelectable
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

---@return Source.Windows._WindowAttrShopSelectable
function WindowAttrShopUI:_getSelectable() end

---@param abilities   table
---@param prices      integer[]
---@param moneyName   string
---@param moneyAmount integer
function WindowAttrShopUI:refreshRows(abilities, prices, moneyName, moneyAmount) end

function WindowAttrShopUI:tick(deltaTime) end

---@param player Source.Player.Player
function WindowAttrShopUI:setPlayer(player) end

---@param attributeName string
---@return string
function WindowAttrShopUI:getAttributeDisplayName(attributeName) end

---@param shopActor       Engine.Actor | nil
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param priceRef        Source.NodeFunctions.Utils.NodeReference<integer | integer[]>
---@param priceIncrement  integer
---@param moneyName       string | nil
---@param rect            sf.IntRect | nil
function WindowAttrShopUI:open(
    shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect
) end

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

---@return integer | integer[]
function WindowAttrShopUI:getPriceValue() end

---@param value integer | integer[]
function WindowAttrShopUI:setPriceValue(value) end

---@return table
function WindowAttrShopUI:getPrices() end

---@param abilityIndex integer
function WindowAttrShopUI:increasePrice(abilityIndex) end

function WindowAttrShopUI:closeAndNotify() end

---@param size integer
---@return sf.IntRect
function WindowAttrShopUI.GetDefaultRect(size) end

---@type Source.UI.WindowAttrShop & Class.ClassType<Source.UI.WindowAttrShop>
local FinalWindowAttrShopUI = WindowAttrShopUI

return FinalWindowAttrShopUI
