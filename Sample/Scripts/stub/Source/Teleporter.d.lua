---@meta Source.Teleporter

---@brief Actor used to move between neighbouring maps in the current region.
---@class Source.Teleporter.Teleporter: Engine.Actor
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
