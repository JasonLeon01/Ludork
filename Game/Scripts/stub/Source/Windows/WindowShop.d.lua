---@meta

---@brief Integrated shop UI with tabs, item list, and item details.
---@class Source.Windows.WindowShop.Controller: Internal.UIBase.UiController
---@field host             Source.Windows.WindowShop
---@field SHOP_MODE_BUY    "buy"
---@field SHOP_MODE_SELL   "sell"
---@field _player          Source.MapActors.Player.Player
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
---@field ui               Internal.UI.WindowShop
local Controller = {}

---@param buyItemIDs table
---@return table
function Controller.NormalizeBuyItems(buyItemIDs) end

---@param itemID string
---@return integer
function Controller.GetItemPrice(itemID) end

---@param itemID string
---@return integer
function Controller.GetSellPrice(itemID) end

---@param player  Source.MapActors.Player.Player
---@param onClose function | nil
function Controller:init(player, onClose) end

---@return Source.Windows.WindowShopTabs
function Controller:getTabWindow() end

---@return Source.Windows.WindowShopItem
function Controller:getItemWindow() end

---@return Source.Windows.WindowShopDetail
function Controller:getDetailWindow() end

---@param player Source.MapActors.Player.Player
function Controller:setPlayer(player) end

---@return boolean
function Controller:getVisible() end

---@return boolean
function Controller:isClosed() end

---@param buyItemIDs table
---@param canSell    boolean
function Controller:open(buyItemIDs, canSell) end

---@param onHidden function | nil
function Controller:close(onHidden) end

function Controller:closeByCancel() end

---@return boolean
function Controller:handleTabNavigationInput() end

---@param index integer
function Controller:onTabSelected(index) end

---@param mode string
function Controller:setMode(mode) end

function Controller:notifyItemIndexMaybeChanged() end

function Controller:refreshLocale() end

function Controller:confirmItem() end
