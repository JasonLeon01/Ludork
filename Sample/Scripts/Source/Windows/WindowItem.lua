local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowItemUI = require("Source.UI.WindowItem")

local WindowItem = {}

function WindowItem:init(rect, player, onClose)
    super(WindowItem, self).init(rect, nil, 32, 32)
    self:setHasReturnBtn(true)
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

function WindowItem:setPlayer(player)
    self._player = player
end

---@param target Engine.Canvas
---@param width  integer
---@param height integer
---@diagnostic disable-next-line: unused
function WindowItem:_resizeCanvas(target, width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
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

function WindowItem:open()
    self._itemUI:open()
end

function WindowItem:refreshLocale()
    self:_updateDescription()
end

function WindowItem:close()
    self._itemUI:close()
end

function WindowItem:onReturn()
    self._itemUI:_closeByCancel()
end

function WindowItem:_onUseItem()
    self._itemUI:_onUseItem()
end

return class(WindowItem, WindowSelectable)
