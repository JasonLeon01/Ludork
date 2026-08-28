local WorldGeometry = {}

---@param left  Global.WorldGeometry.CellRect
---@param right Global.WorldGeometry.CellRect
---@return boolean
function WorldGeometry.RectIntersects(left, right)
    return left.x < right.x + right.width and left.x + left.width > right.x
        and left.y < right.y + right.height and left.y + left.height > right.y
end

---@param rect     Global.WorldGeometry.CellRect
---@param position sf.Vector2i
---@return boolean
function WorldGeometry.RectContainsPosition(rect, position)
    return position.x >= rect.x and position.y >= rect.y
        and position.x < rect.x + rect.width and position.y < rect.y + rect.height
end

---@param x integer
---@param y integer
---@return string
function WorldGeometry.GridKey(x, y)
    return tostring(x) .. "," .. tostring(y)
end

---@param position  sf.Vector2i
---@param chunkSize integer
---@return string
function WorldGeometry.CellChunkKey(position, chunkSize)
    return WorldGeometry.GridKey(math.floor(position.x / chunkSize), math.floor(position.y / chunkSize))
end

return WorldGeometry
