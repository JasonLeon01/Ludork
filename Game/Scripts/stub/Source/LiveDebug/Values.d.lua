---@meta Source.LiveDebug.Values

local Values = {}

--- Read supported ordinary metadata fields from a live Actor, excluding component structure and identity fields.
--- Schema metadata uses JSON arrays and canonical type names.
--- Runtime references, Optional schemas and values outside the bounded scalar, SFML value, container and known data-record surface are read-only summaries.
---@param actor   Engine.Actor
---@param actorId string
---@return Source.LiveDebug.Info
function Values.Read(actor, actorId) end

--- Apply a validated live typed field, refreshing its runtime presentation or condition subscription.
--- Both the existing value and replacement must fit the same bounded data-value surface; runtime object graphs are never traversed.
---@param actor Engine.Actor
---@param name  string
---@param value any
function Values.Write(actor, name, value) end

return Values
