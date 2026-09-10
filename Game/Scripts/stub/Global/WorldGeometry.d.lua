---@meta Global.WorldGeometry

---@class Global.WorldGeometry.CellRect
---@field x      integer
---@field y      integer
---@field width  integer
---@field height integer

---@class Global.WorldGeometry.Module
local WorldGeometry = {}

---@param left  Global.WorldGeometry.CellRect
---@param right Global.WorldGeometry.CellRect
---@return boolean
function WorldGeometry.RectIntersects(left, right) end

---@param rect     Global.WorldGeometry.CellRect
---@param position sf.Vector2i
---@return boolean
function WorldGeometry.RectContainsPosition(rect, position) end

---@param x integer
---@param y integer
---@return string
function WorldGeometry.GridKey(x, y) end

---@param position  sf.Vector2i
---@param chunkSize integer
---@return string
function WorldGeometry.CellChunkKey(position, chunkSize) end

return WorldGeometry
