---@meta Source.Windows.WindowAttrShop

---@brief Attribute upgrade shop coordinator.
---@class Source.Windows.WindowAttrShop
---@field new              fun(player: Source.Player.Player, onClose?: function): Source.Windows.WindowAttrShop
---@field uiClass          Class.ClassType<Source.UI.WindowAttrShop>
---@field _player          Source.Player.Player
---@field _onCloseCallback function | nil
---@field _abilities       table<string, integer>
---@field _abilityKeys     string[]
---@field _priceRef        Source.NodeFunctions.Utils.NodeReference<integer | integer[]> | nil
---@field _fallbackPrice   integer
---@field _priceIncrement  integer
---@field _moneyName       string
---@field _closed          boolean
---@field _shopUI          Source.UI.WindowAttrShop
---@field _selectable      Source.Windows.WindowAttrShop.Selectable
local WindowAttrShop = {}

---@private
---@return integer | integer[]
function WindowAttrShop:_getPriceValue() end

---@private
---@param value integer | integer[]
function WindowAttrShop:_setPriceValue(value) end

---@private
---@return integer[]
function WindowAttrShop:_getPrices() end

---@private
---@param abilityIndex integer
function WindowAttrShop:_increasePrice(abilityIndex) end

---@return sf.IntRect
function WindowAttrShop.GetDefaultRect() end

---@brief Construct the attribute shop.
---
--- - @param player Player whose currency and attributes are modified.
--- - @param onClose Callback invoked after the shop closes.
---@param player  Source.Player.Player
---@param onClose function | nil
function WindowAttrShop:init(player, onClose) end

---@brief Get the shop selection window for UI manager registration.
---@return Source.Windows.WindowAttrShop.Selectable
function WindowAttrShop:getSelectable() end

---@brief Get the player currently bound to the shop.
---@return Source.Player.Player
function WindowAttrShop:getPlayer() end

---@brief Rebind the player used by the shop.
---
--- - @param player New player instance.
---@param player Source.Player.Player
function WindowAttrShop:setPlayer(player) end

---@brief Resolve a display name for a player info component attribute.
---
--- - @param attributeName Player info component attribute name.
--- - @return Localised display name.
---@param attributeName string
---@return string
function WindowAttrShop:getAttributeDisplayName(attributeName) end

---@brief Open the shop with the supplied actor, text, abilities, price, and first ability selected.
---
--- - @param shopActor Actor whose first texture frame is used as the avatar.
--- - @param shopName Locale key for the shop name.
--- - @param shopDescription Locale key for the shop description.
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param priceRef Mutable reference containing a shared scalar price or per-offer prices in offer order; nil uses an internal shared price starting at zero.
--- - @param priceIncrement Amount added to the shared price or the purchased offer's price after each purchase.
--- - @param moneyName Player info component attribute used as currency.
--- - @param rect Optional centred shop rectangle.
---@param shopActor       Engine.Actor | nil
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param priceRef        Source.NodeFunctions.Utils.NodeReference<integer | integer[]> | nil
---@param priceIncrement  integer
---@param moneyName       string | nil
---@param rect            sf.IntRect | nil
function WindowAttrShop:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect) end

---@brief Refresh the shared price label for scalar prices.
function WindowAttrShop:refreshPriceText() end

---@brief Refresh ability availability and displayed prices.
function WindowAttrShop:refreshItems() end

---@brief Refresh localised shop text, price text, and ability rows without changing the current selection.
function WindowAttrShop:refreshLocale() end

---@brief Close and deactivate the attribute shop.
---@param notify boolean | nil
function WindowAttrShop:close(notify) end

---@brief Close the shop via cancel input and notify its owner.
function WindowAttrShop:closeByCancel() end

---@brief Confirm the selected attribute purchase or Leave command.
function WindowAttrShop:confirmItem() end

---@brief Return whether the shop is visible.
---@return boolean
function WindowAttrShop:getVisible() end

---@brief Return whether the latest shop session has closed.
---@return boolean
function WindowAttrShop:isClosed() end

---@param deltaTime number
function WindowAttrShop:animateAvatar(deltaTime) end

---@param abilityKey       string
---@param delta            integer
---@param price            integer
---@param moneyDisplayName string | nil
---@return string
function WindowAttrShop:formatPurchaseText(abilityKey, delta, price, moneyDisplayName) end

---@class Source.Windows.WindowAttrShop.Offer
---@field key       string
---@field delta     integer
---@field price     integer
---@field available boolean

---@brief Return detached offers in their configured order, with current price and affordability.
---@return Source.Windows.WindowAttrShop.Offer[]
function WindowAttrShop:getOffers() end

---@brief Validate and purchase one attribute, update Base values together, then increase its price.
---@param key string
---@return boolean
function WindowAttrShop:purchaseAttribute(key) end

---@return string
function WindowAttrShop:getCurrencyName() end

---@return integer | nil
function WindowAttrShop:getSharedPrice() end

return WindowAttrShop
