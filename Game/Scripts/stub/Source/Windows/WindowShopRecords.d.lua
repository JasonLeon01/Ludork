---@meta

---@class Source.Windows.WindowShopRecords.Controller: Internal.UIBase.UiController
---@field host               Source.Windows.WindowShopRecords
---@field ui                 Internal.UI.WindowShopRecords
---@field _shops             Source.GameInstance.ShopRecord[]
---@field _currentMapPath    string | nil
---@field _rows              Internal.UIBase.UiCollection<Source.Windows.Shared.SelectableTextRow.Controller>
---@field _onConfirmCallback fun(mapPath: string, tag: string) | nil
---@field _onCloseCallback   function | nil
local Controller = {}

---@param onConfirm fun(mapPath: string, tag: string) | nil
---@param onClose   function | nil
function Controller:init(onConfirm, onClose) end

---@return boolean
function Controller:getVisible() end

--- Open the recorded shops at the first row. An empty list does not open the window.
---@param shops          Source.GameInstance.ShopRecord[]
---@param currentMapPath string | nil
---@return boolean
function Controller:open(shops, currentMapPath) end

---@param shop Source.GameInstance.ShopRecord
---@return string
function Controller:formatShopName(shop) end

--- Update row labels without changing selection or scroll position.
function Controller:refreshLocale() end

function Controller:confirmShop() end

--- Close without reporting a user cancellation.
---@param onHidden function | nil
function Controller:close(onHidden) end

--- Hide immediately without reporting a user cancellation.
function Controller:dismiss() end

function Controller:onReturn() end

function Controller:dispose() end
