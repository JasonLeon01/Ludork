local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowItemUI = require("Source.UI.WindowItem")

local WindowItem = {}

-- Load an item icon texture from the icon file path.
---
--- - @param iconPath The icon file path from GeneralData.
--- - @return Loaded Texture, or None when iconPath is empty.
---@param iconPath string
---@return sf.Texture | nil
function WindowItem._loadItemIcon(iconPath)
    return WindowItemUI.loadItemIcon(iconPath)
end

function WindowItem:init(rect, player, onClose)
    super(WindowItem, self).init(rect, nil, 32, 32)
    self._onCloseCallback = onClose
    self._onUseCallback = nil
    self._player = player
    self._lastDescIndex = nil
    self._itemList = {}
    self._itemUI = WindowItemUI.new(self)
    self._itemUI:attach()
    self:_refreshItems()
    self:setActive(false)
    self:setVisible(false)
end

---@param target Engine.Canvas
---@param width  integer
---@param height integer
function WindowItem:_resizeCanvas(target, width, height)
    target:resize(sf.Vector2u.new(width, height))
    target:setView(target:getDefaultView())
end

-- Rebuild the item list from the player's current inventory.
function WindowItem:_refreshItems()
    self._itemUI:refreshItems()
end

function WindowItem:onTick(deltaTime)
    super(WindowItem, self).onTick(deltaTime)
    self._itemUI:tick()
end

---@param text string
---@return string
function WindowItem:_wrapDesc(text)
    return self._itemUI:wrapDescription(text)
end

function WindowItem:_updateDescription()
    self._itemUI:updateDescription()
end

function WindowItem:onKeyDown(kwargs)
    if self._itemUI:handleKeyDown() then
        return
    end
    return super(WindowItem, self).onKeyDown(kwargs)
end

function WindowItem:onMouseButtonDown(kwargs)
    return self._itemUI:handleMouseButtonDown(kwargs)
end

function WindowItem:open()
    self._itemUI:open()
end

function WindowItem:close()
    self._itemUI:close()
end

function WindowItem:_onUseItem()
    self._itemUI:_onUseItem()
end

function WindowItem:_closeByCancel()
    self._itemUI:_closeByCancel()
end

return class(WindowItem, WindowSelectable)
