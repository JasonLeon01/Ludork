---@meta Source.Windows.WindowShopCommand

--- @brief Horizontal buy/sell command bar for the shop.
---@class Source.Windows.WindowShopCommand: Source.Windows.WindowCommand
---@field controllerClass Source.Windows.WindowShopCommandController
---@field _owner Source.Windows.WindowShop
---@field _lastIndex integer | nil
local WindowShopCommand = {}

---@param rect sf.IntRect
---@param owner Source.Windows.WindowShop
---@return Source.Windows.WindowShopCommand
function WindowShopCommand.new(rect, owner) end

--- @brief Construct the shop command bar.
---
--- - @param rect The command window rectangle.
--- - @param owner The shop coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowShop
function WindowShopCommand:init(rect, owner) end

---@param deltaTime number
function WindowShopCommand:onTick(deltaTime) end

---@param kwargs table
function WindowShopCommand:onKeyDown(kwargs) end

---@param kwargs table
---@return boolean
function WindowShopCommand:onMouseButtonDown(kwargs) end

return WindowShopCommand
