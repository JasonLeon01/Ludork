---@meta Source.Teleporter

---@brief Actor used to move between neighbouring floors or to a chosen map position.
---@class Source.Teleporter.Teleporter: Source.ConditionalActor
---@field Offset                sf.Vector2i
---@field stairSE               string
---@field transitionName        string
---@field transitionTime        number
---@field _floorTransferPending boolean
local Teleporter = {}

---@brief Initialise a teleporter actor.
---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Teleporter:init(texture, rect, tag) end

---@brief Move to the next map in the current region.
function Teleporter:goUpstairs() end

---@brief Move to the previous map in the current region.
function Teleporter:goDownstairs() end

---@brief Move to a chosen map tile without searching for a destination stair.
--- Records this actor's position including Offset and raw tag on the source map,
--- and the exact arrival position with an empty tag on the destination map.
--- Each map retains its own telepoints and region membership. Passing false leaves both records unchanged.
---@param mapPath  string
---@param position sf.Vector2i
---@param record?  boolean     Defaults to true.
function Teleporter:goToMap(mapPath, position, record) end

---@brief Get this teleporter's map position plus Offset.
---
--- - @return Target tile position as `sf.Vector2i`.
---@return sf.Vector2i
function Teleporter:getTeleportPosition() end

---@brief Whether `position` is on or orthogonally adjacent to a teleporter.
---
--- - @param actors Actors to search.
--- - @param position Map tile position to test.
--- - @return True when a teleporter shares the tile or is one step away (4-dir).
---@param actors   Engine.Actor[]
---@param position sf.Vector2i
---@return boolean
function Teleporter.IsAsideOrOverlapping(actors, position) end

---@param actors   Engine.Actor[]
---@param position sf.Vector2i
---@return Source.Teleporter.Teleporter | nil
function Teleporter.FindNearestTeleporter(actors, position) end

---@param regionMaps string[]
---@param currentMap string
---@return integer | nil
function Teleporter.FindCurrentMapIndex(regionMaps, currentMap) end

return Teleporter
