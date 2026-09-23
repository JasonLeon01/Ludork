---@meta

---@class Source.Windows.WindowAttrShop.Offer
---@field key       string
---@field delta     integer
---@field price     integer
---@field available boolean

---@brief Attribute upgrade shop window and Controller.
---@class Source.Windows.WindowAttrShop.Controller: Internal.UIBase.UiController
---@field _player               Source.MapActors.Player.Player
---@field _onCloseCallback      function | nil
---@field _abilities            table<string, integer>
---@field _abilityKeys          string[]
---@field _priceRef             GlobalFunctions.Utils.NodeReference<integer | integer[]> | nil
---@field _fallbackPrice        integer
---@field _priceIncrement       integer
---@field _moneyName            string
---@field _closed               boolean
---@field ui                    Internal.UI.WindowAttrShop
---@field _shopNameSource       string
---@field _descriptionSource    string
---@field _shopName             string
---@field _description          string
---@field _priceTextValue       string
---@field _avatarTexture        sf.Texture | nil
---@field _avatarRect           sf.IntRect | nil
---@field _avatarAnimatable     boolean
---@field _avatarSwitchInterval number
---@field _avatarSwitchTimer    number
---@field _offers               Source.Windows.WindowAttrShop.Offer[]
---@field _rows                 Internal.UIBase.UiCollection<Source.Windows.WindowAttrShop.AttrShopRow.Controller>
---@field host                  Source.Windows.WindowAttrShop
local Controller = {}

---@private
---@return integer | integer[]
function Controller:_getPriceValue() end

---@private
---@param value integer | integer[]
function Controller:_setPriceValue(value) end

---@private
---@return integer[]
function Controller:_getPrices() end

---@private
---@param abilityIndex integer
function Controller:_increasePrice(abilityIndex) end

---@brief Construct the attribute shop.
---
--- - @param player Player whose currency and attributes are modified.
--- - @param onClose Callback invoked after the shop closes.
---@param player  Source.MapActors.Player.Player
---@param onClose function | nil
function Controller:init(player, onClose) end

---@brief Get the player currently bound to the shop.
---@return Source.MapActors.Player.Player
function Controller:getPlayer() end

---@brief Rebind the player used by the shop.
---
--- - @param player New player instance.
---@param player Source.MapActors.Player.Player
function Controller:setPlayer(player) end

---@brief Resolve a display name for a player info component attribute.
---
--- - @param attributeName Player info component attribute name.
--- - @return Localised display name.
---@param attributeName string
---@return string
function Controller:getAttributeDisplayName(attributeName) end

---@brief Open the shop with the supplied actor, text, abilities, price, and first ability selected.
---
--- - @param shopActor Actor whose first texture frame is used as the avatar.
--- - @param shopName Locale key for the shop name.
--- - @param shopDescription Locale key for the shop description.
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param priceRef Mutable reference containing a shared scalar price or per-offer prices in offer order; nil uses an internal shared price starting at zero.
--- - @param priceIncrement Amount added to the shared price or the purchased offer's price after each purchase.
--- - @param moneyName Player info component attribute used as currency.
---@param shopActor       Engine.Actor | nil
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param priceRef        GlobalFunctions.Utils.NodeReference<integer | integer[]> | nil
---@param priceIncrement  integer
---@param moneyName       string | nil
function Controller:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName) end

---@brief Refresh the shared price label for scalar prices.
function Controller:refreshPriceText() end

---@brief Refresh ability availability and displayed prices.
function Controller:refreshItems() end

---@brief Refresh localised shop text, price text, and ability rows without changing the current selection.
function Controller:refreshLocale() end

---@brief Close and deactivate the attribute shop.
---@param notify boolean | nil
function Controller:close(notify) end

---@brief Close the shop via cancel input and notify its owner.
function Controller:closeByCancel() end

---@brief Confirm the selected attribute purchase or Leave command.
function Controller:confirmItem() end

---@brief Return whether the latest shop session has closed.
---@return boolean
function Controller:isClosed() end

---@param deltaTime number
function Controller:animateAvatar(deltaTime) end

---@param abilityKey       string
---@param delta            integer
---@param price            integer
---@param moneyDisplayName string | nil
---@return string
function Controller:formatPurchaseText(abilityKey, delta, price, moneyDisplayName) end

---@brief Return detached offers in their configured order, with current price and affordability.
---@return Source.Windows.WindowAttrShop.Offer[]
function Controller:getOffers() end

---@brief Validate and purchase one attribute, update Base values together, then increase its price.
---@param key string
---@return boolean
function Controller:purchaseAttribute(key) end

---@return string
function Controller:getCurrencyName() end

---@return integer | nil
function Controller:getSharedPrice() end

function Controller:dispose() end

function Controller:refresh() end

---@param shopActor Engine.Actor | nil
function Controller:refreshAvatar(shopActor) end

function Controller:refreshRows() end

---@return string | nil
function Controller:getSelectedAbilityKey() end

---@return boolean
function Controller:isCurrentAvailable() end

---@param deltaTime number
function Controller:onTick(deltaTime) end

function Controller:onReturn() end
