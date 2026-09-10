local Engine = require("Engine")

local AutoTileRuntime = {}

---@param texture sf.Texture | nil
---@return integer
function AutoTileRuntime.GetFrameCount(texture)
    if texture == nil then
        return 1
    end
    local size = texture:getSize()
    local frames = Engine.GetCellSize() > 0 and math.floor(size.x / (3 * Engine.GetCellSize())) or 1
    return math.max(frames, 1)
end

return AutoTileRuntime
