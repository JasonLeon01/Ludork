local UiControlFactory = require("Source.UI.UiControlFactory")

---@class Source.UI.Helpers.ListView
local ListViewController = {}

function ListViewController:init(model, logicalSize, defaultItemHeight, fixItemHeight, columns)
    self.model = model
    self._logicalSize = logicalSize or sf.Vector2u.new(1, 1)
    self._defaultItemHeight = defaultItemHeight or 32
    if fixItemHeight == nil then
        fixItemHeight = true
    end
    self._fixItemHeight = fixItemHeight
    self._columns = columns or 1
    self.root = UiControlFactory.createListView(
        self._logicalSize, self._defaultItemHeight, self._fixItemHeight, self._columns
    )
    self._bound = false
end

function ListViewController:bind()
end

function ListViewController:refresh()
end

function ListViewController:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    self.root:setSize(
        sf.Vector2i.new(math.max(1, math.floor(self._logicalSize.x)), math.max(1, math.floor(self._logicalSize.y)))
    )
    self.root:setColumns(self._columns)
    if not self._bound then
        self:bind()
        self._bound = true
    end
    self:refresh()
    self.root:applyPositions()
    return self.root
end

function ListViewController:getListView()
    return self.root
end

return class(ListViewController)
