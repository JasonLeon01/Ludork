local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowItemUI = require("Source.UI.WindowItem")

---@class Source.Windows.WindowItem
local WindowItem = {}

function WindowItem:init(rect, player, onClose)
    super(WindowItem, self).init(rect, nil, 32, 32, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._onCloseCallback = onClose
    self._onUseCallback = nil
    self._player = player
    self._itemUI = WindowItemUI.new(self)
    self._itemUI:attach()
    self:_refreshItems()
    self:hideImmediate()
end

function WindowItem:setPlayer(player)
    self._player = player
end

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

function WindowItem:close(onHidden)
    self._itemUI:close(onHidden)
end

function WindowItem:onReturn()
    self._itemUI:closeByCancel()
end

function WindowItem:_onUseItem()
    self._itemUI:useSelectedItem()
end

function WindowItem:getPlayer()
    return self._player
end

function WindowItem:setOnCloseCallback(callback)
    self._onCloseCallback = callback
end

function WindowItem:setOnUseCallback(callback)
    self._onUseCallback = callback
end

function WindowItem:onItemUsed()
    if self._onUseCallback ~= nil then
        self._onUseCallback()
    end
end

function WindowItem:notifyClosed()
    if self._onCloseCallback ~= nil then
        self._onCloseCallback()
    end
end

return class(WindowItem, WindowSelectable)
