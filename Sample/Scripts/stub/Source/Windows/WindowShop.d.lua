---@meta Source.Windows.WindowShop

---@brief Integrated shop UI with tabs, item list, and item details.
---@class Source.Windows.WindowShop: Engine.Canvas
---@field SHOP_MODE_BUY    "buy"
---@field SHOP_MODE_SELL   "sell"
---@field _player          Source.Player.Player
---@field _onCloseCallback function | nil
---@field _tabWindow       Source.Windows.WindowShopTabs
---@field _itemWindow      Source.Windows.WindowShopItem
---@field _detailWindow    Source.Windows.WindowShopDetail
---@field _tabTopLeft      sf.Vector2f
---@field _itemTopLeft     sf.Vector2f
---@field _detailTopLeft   sf.Vector2f
---@field _buyItemIDs      string[]
---@field _canSell         boolean
---@field _mode            string
---@field _closed          boolean
---@field new              fun(player: Source.Player.Player, tabRect?: sf.IntRect, itemRect?: sf.IntRect, detailRect?: sf.IntRect, onClose?: function): Source.Windows.WindowShop
local WindowShop = {}

---@return sf.IntRect, sf.IntRect, sf.IntRect
function WindowShop.GetDefaultRects() end

---@param buyItemIDs table
---@return table
function WindowShop.NormalizeBuyItems(buyItemIDs) end

---@param itemID string
---@return integer
function WindowShop.GetItemPrice(itemID) end

---@param itemID string
---@return integer
function WindowShop.GetSellPrice(itemID) end

---@param player     Source.Player.Player
---@param tabRect    sf.IntRect | nil
---@param itemRect   sf.IntRect | nil
---@param detailRect sf.IntRect | nil
---@param onClose    function | nil
function WindowShop:init(player, tabRect, itemRect, detailRect, onClose) end

---@return Source.Windows.WindowShopTabs
function WindowShop:getTabWindow() end

---@return Source.Windows.WindowShopItem
function WindowShop:getItemWindow() end

---@return Source.Windows.WindowShopDetail
function WindowShop:getDetailWindow() end

---@param player Source.Player.Player
function WindowShop:setPlayer(player) end

---@return boolean
function WindowShop:getVisible() end

---@return boolean
function WindowShop:isClosed() end

---@param buyItemIDs table
---@param canSell    boolean
function WindowShop:open(buyItemIDs, canSell) end

---@param onHidden function | nil
function WindowShop:close(onHidden) end

function WindowShop:closeByCancel() end

---@return boolean
function WindowShop:handleTabNavigationInput() end

---@param index integer
function WindowShop:onTabSelected(index) end

---@param mode string
function WindowShop:setMode(mode) end

function WindowShop:notifyItemIndexMaybeChanged() end

function WindowShop:refreshLocale() end

function WindowShop:confirmItem() end

return WindowShop
