---@meta Source.Windows.WindowAttrShop
---@class Source.Windows._WindowAttrShopSelectable: Source.Windows.Base.WindowSelectable
---@field _owner Source.Windows.WindowAttrShop
---@field _abilityKeys string[]
---@field _cellAvailable boolean[]
---@field _listView Engine.ListView
local _WindowAttrShopSelectable = {}

--- @brief Construct the attribute shop selection window.
---
--- - @param rect Window rectangle.
--- - @param owner Attribute shop coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowAttrShop
function _WindowAttrShopSelectable:init(rect, owner) end

--- @brief Rebuild the ability rows and leave command.
---
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param prices Purchase prices ordered to match abilities.
--- - @param moneyName Player info component attribute used as currency.
--- - @param moneyAmount Current amount of the selected currency.
---@param abilities   table
---@param prices      table
---@param moneyName   string
---@param moneyAmount integer
function _WindowAttrShopSelectable:refresh(abilities, prices, moneyName, moneyAmount) end

--- @brief Get the selected player attribute name.
---
--- - @return Attribute name, or nil when Leave is selected.
---@return string | nil
function _WindowAttrShopSelectable:getSelectedAbilityKey() end

--- @brief Return whether the selected row can be confirmed.
---@return boolean
function _WindowAttrShopSelectable:isCurrentAvailable() end

---@param deltaTime number
function _WindowAttrShopSelectable:onTick(deltaTime) end

---@param kwargs table
function _WindowAttrShopSelectable:onKeyDown(kwargs) end

---@param kwargs table
---@return boolean
function _WindowAttrShopSelectable:onMouseButtonDown(kwargs) end

--- @brief Attribute upgrade shop coordinator.
---@class Source.Windows.WindowAttrShop
---@field uiClass Class.ClassType<Source.UI.WindowAttrShop>
---@field _player Source.Player.Player
---@field _onCloseCallback function | nil
---@field _abilities table<string, integer>
---@field _abilityKeys string[]
---@field _priceRef Source.NodeFunctions.Utils.NodeReference<integer | integer[]> | nil
---@field _fallbackPrice integer
---@field _priceIncrement integer
---@field _moneyName string
---@field _closed boolean
---@field _avatarTexture sf.Texture | nil
---@field _avatarRect sf.IntRect | nil
---@field _avatarAnimatable boolean
---@field _avatarSwitchInterval number
---@field _avatarSwitchTimer number
---@field _avatarImage Engine.Image
---@field _nameText Engine.PlainText
---@field _descText Engine.PlainText
---@field _priceText Engine.PlainText
---@field _shopUI Source.UI.WindowAttrShop
---@field _selectable Source.Windows._WindowAttrShopSelectable
local WindowAttrShop = {}

---@return sf.IntRect
function WindowAttrShop.GetDefaultRect() end

--- @brief Construct the attribute shop.
---
--- - @param player Player whose currency and attributes are modified.
--- - @param onClose Callback invoked after the shop closes.
---@param player  Source.Player.Player
---@param onClose function | nil
function WindowAttrShop:init(player, onClose) end

--- @brief Get the shop selection window for UI manager registration.
---@return Source.Windows._WindowAttrShopSelectable
function WindowAttrShop:getSelectable() end

--- @brief Get the player currently bound to the shop.
---@return Source.Player.Player
function WindowAttrShop:getPlayer() end

--- @brief Rebind the player used by the shop.
---
--- - @param player New player instance.
---@param player Source.Player.Player
function WindowAttrShop:setPlayer(player) end

--- @brief Resolve a display name for a player info component attribute.
---
--- - @param attributeName Player info component attribute name.
--- - @return Localised display name.
---@param attributeName string
---@return string
function WindowAttrShop:getAttributeDisplayName(attributeName) end

--- @brief Open the shop with the supplied actor, text, abilities, and price.
---
--- - @param shopActor Actor whose first texture frame is used as the avatar.
--- - @param shopName Locale key for the shop name.
--- - @param shopDescription Locale key for the shop description.
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param priceRef Mutable reference containing the current shared price.
--- - @param priceIncrement Amount added to the shared price after each purchase.
--- - @param moneyName Player info component attribute used as currency.
--- - @param rect Optional centred shop rectangle.
---@param shopActor       Engine.Actor | nil
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param priceRef        Source.NodeFunctions.Utils.NodeReference<integer | integer[]>
---@param priceIncrement  integer
---@param moneyName       string | nil
---@param rect            sf.IntRect | nil
function WindowAttrShop:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect) end

--- @brief Refresh the shared price label for scalar prices.
function WindowAttrShop:refreshPriceText() end

--- @brief Refresh ability availability and displayed prices.
function WindowAttrShop:refreshItems() end

--- @brief Refresh localised shop text, price text, and ability rows without changing the current selection.
function WindowAttrShop:refreshLocale() end

--- @brief Close and deactivate the attribute shop.
function WindowAttrShop:close() end

--- @brief Close the shop via cancel input and notify its owner.
function WindowAttrShop:closeByCancel() end

--- @brief Confirm the selected attribute purchase or Leave command.
function WindowAttrShop:confirmItem() end

--- @brief Return whether the shop is visible.
---@return boolean
function WindowAttrShop:getVisible() end

--- @brief Return whether the latest shop session has closed.
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

---@alias WindowAttrShopSelectableType Source.Windows._WindowAttrShopSelectable

---@class Source.Windows.WindowAttrShopExports
---@field _WindowAttrShopSelectable WindowAttrShopSelectableType & Class.ClassType<WindowAttrShopSelectableType>
---@field WindowAttrShop            Source.Windows.WindowAttrShop & Class.ClassType<Source.Windows.WindowAttrShop>
local WindowAttrShopExports = {}

return WindowAttrShopExports
